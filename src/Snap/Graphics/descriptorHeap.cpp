#include "descriptorHeap.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/units.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

static const VkDeviceSize maxResourceHeapSize = Units::Memory::MiBToBytes(16);
static const VkDeviceSize maxSamplerHeapSize = Units::Memory::MiBToBytes(1);

auto DescriptorHeapManager::Initialize(const GraphicsContext &context)
    -> Error {
  heapProperties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT;

  VkPhysicalDeviceProperties2 deviceProps2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &heapProperties,
  };

  vkGetPhysicalDeviceProperties2(context.physicalDevice, &deviceProps2);

  VkDeviceSize resourceHeapSize = heapProperties.maxResourceHeapSize;
  if (resourceHeapSize == 0ULL) {
    resourceHeapSize = maxResourceHeapSize;
  }

  VkDeviceSize samplerHeapSize = heapProperties.maxSamplerHeapSize;
  if (samplerHeapSize == 0ULL) {
    samplerHeapSize = maxSamplerHeapSize;
  }

  // clang-format off
  ResourceHeap = CHECK_RES(Buffer::Create(context, {
    .size = std::min(resourceHeapSize, maxResourceHeapSize),
    .usage = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
    .stagingBuffer = false,
    .persistentMapping = false,
    .debugName = "Resource heap",
  }));

  SamplerHeap = CHECK_RES(Buffer::Create(context, {
    .size = std::min(samplerHeapSize, maxSamplerHeapSize),
    .usage = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
    .stagingBuffer = false,
    .persistentMapping = false,
    .debugName = "Sampler heap",
  }));
  // clang-format on

  resourceAddress = CHECK_RES(ResourceHeap->GetDeviceAddress(context));
  samplerAddress = CHECK_RES(SamplerHeap->GetDeviceAddress(context));

  auto bufferAlignedDownToSize =
      Utils::AlignDown(ResourceHeap->size - heapProperties.bufferDescriptorSize,
                       heapProperties.bufferDescriptorSize);

  if (heapProperties.bufferDescriptorAlignment != 0) {
    BufferCursorOrigin = Utils::AlignDown(
        bufferAlignedDownToSize, heapProperties.bufferDescriptorAlignment);
  } else {
    BufferCursorOrigin = bufferAlignedDownToSize;
  }

  samplerCursor = heapProperties.minSamplerHeapReservedRange;
  imageCursor = heapProperties.minResourceHeapReservedRange;

  return {};
}

auto DescriptorHeapManager::DeInitialize() -> void {
  ResourceHeap.reset();
  SamplerHeap.reset();
}

auto DescriptorHeapManager::AllocateResource(bool isImage)
    -> Result<VkDeviceSize> {
  auto *memoryManager = isImage ? &imageMemoryManager : &bufferMemoryManager;
  VkDeviceSize size = isImage ? heapProperties.imageDescriptorSize
                              : heapProperties.bufferDescriptorSize;

  auto addressFromGap = CHECK_RES(memoryManager->TryAllocateFromGap(size));

  if (addressFromGap.has_value()) {
    auto offset = addressFromGap.value();

    if (!isImage) {
      // Resource allocator does not work in reverse, so we just reverse the pointer at the end.
      offset = BufferCursorOrigin - offset;
    }

    return offset;
  }

  // We do not need align with heapProperties.imageDescriptorAlignment / heapProperties.bufferDescriptorAlignment
  // As "the spec requires that the descriptorAlignment is less than or equal to the *descriptorSize, so you are always able to tightly pack an array of descriptors together."
  // meaning that as long as the cursor origins were aligned to start with (0 and aligned down size) then we can never get out of alignment.

  auto &cursor = isImage ? imageCursor : bufferCursor;
  auto alignment = isImage ? heapProperties.imageDescriptorAlignment
                           : heapProperties.bufferDescriptorAlignment;

  VkDeviceSize offset = Utils::AlignUp(cursor, alignment);
  if (!isImage) {
    offset = Utils::AlignDown(BufferCursorOrigin - cursor, alignment);
  }

  cursor = Utils::AlignUp(cursor + size, alignment);

  ERR_ASSERT_MSG(imageCursor + heapProperties.imageDescriptorSize <
                     BufferCursorOrigin -
                         (bufferCursor + heapProperties.bufferDescriptorSize),
                 "Out of heap memory");

  return offset;
}

auto DescriptorHeapManager::AllocateSampler() -> Result<VkDeviceSize> {
  VkDeviceSize size = heapProperties.samplerDescriptorSize;
  auto addressFromGap =
      CHECK_RES(samplerMemoryManager.TryAllocateFromGap(size));

  if (addressFromGap.has_value()) {
    return addressFromGap.value();
  }

  VkDeviceSize offset = samplerCursor;
  ERR_ASSERT_MSG(offset + size < SamplerHeap->size, "Out of heap memory");

  // Same alignment argument as Resource allocation.
  samplerCursor = samplerCursor + size;

  return offset;
}

auto DescriptorHeapManager::AllocateResource(const GraphicsContext &context,
                                             const Buffer *buffer,
                                             VkDescriptorType type,
                                             uint32_t offset, uint32_t size)
    -> Result<VkDeviceSize> {

  auto deviceAddress = CHECK_RES(buffer->GetDeviceAddress(context));

  VkDeviceAddressRangeKHR addressRange{
      .address = deviceAddress + offset,
      .size = size,
  };

  PrintAlways("Allocating: offset: {}, size: {}, type: {}", offset, size,
              (int)type);

  VkResourceDescriptorInfoEXT resourceInfo{
      .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
      .pNext = nullptr,
      .type = type,
      .data = {.pAddressRange = &addressRange}};

  VkHostAddressRangeEXT hostAddressRange{};
  VkDeviceSize address{};
  std::vector<uint8_t> descriptor(heapProperties.bufferDescriptorSize);

  {
    // TODO: Own a lock for protecting allocate instead of relying on the device lock
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    address = CHECK_RES(AllocateResource(false));

    assert(heapProperties.bufferDescriptorSize <= 1024);

    hostAddressRange.address = descriptor.data();
    hostAddressRange.size = descriptor.size();

    CHECK_NEW_ERR(vkWriteResourceDescriptorsEXT(
        context.device, 1, &resourceInfo, &hostAddressRange));
  }

  CHECK_ERR(ResourceHeap->SetData(context, descriptor, address,
                                  heapProperties.bufferDescriptorSize));

  auto divided = address / heapProperties.bufferDescriptorSize;

  return divided;
}

auto DescriptorHeapManager::AllocateResource(const GraphicsContext &context,
                                             const Texture *texture,
                                             VkDescriptorType type)
    -> Result<VkDeviceSize> {

  VkImageDescriptorInfoEXT imageInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
      .pNext = nullptr,
      .pView = &texture->viewCreateInfo,
      .layout = VK_IMAGE_LAYOUT_GENERAL,
  };

  VkResourceDescriptorInfoEXT resourceInfo{
      .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
      .pNext = nullptr,
      .type = type,
      .data = {.pImage = &imageInfo}};

  VkHostAddressRangeEXT hostAddressRange{};
  VkDeviceSize address{};

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    address = CHECK_RES(AllocateResource(true));

    assert(heapProperties.imageDescriptorSize <= 1024);
    std::vector<uint8_t> descriptor(heapProperties.imageDescriptorSize);

    hostAddressRange.address = descriptor.data();
    hostAddressRange.size = heapProperties.imageDescriptorSize;

    CHECK_NEW_ERR(vkWriteResourceDescriptorsEXT(
        context.device, 1, &resourceInfo, &hostAddressRange));

    CHECK_ERR(ResourceHeap->SetData(context, descriptor, address));
  }

  auto divided = address / heapProperties.imageDescriptorSize;

  return divided;
}

auto DescriptorHeapManager::AllocateSampler(
    const GraphicsContext &context, VkSamplerCreateInfo samplerCreateInfo)
    -> Result<VkDeviceSize> {

  VkHostAddressRangeEXT hostAddressRange{};

  CHECK_NEW_ERR(vkWriteSamplerDescriptorsEXT(
      context.device, 1, &samplerCreateInfo, &hostAddressRange));

  VkDeviceSize address = CHECK_RES(AllocateSampler());
  std::span<uint8_t> span = {
      reinterpret_cast<uint8_t *>(hostAddressRange.address), // NOLINT
      hostAddressRange.size};

  CHECK_ERR(SamplerHeap->SetData(context, span, address));

  return address;
}

auto DescriptorHeapManager::FreeImageResourceIndex(VkDeviceSize resourcePtr)
    -> void {
  resourcePtr *= heapProperties.imageDescriptorSize;

  assert(resourcePtr <= imageCursor); // Cannot free outside of used range
  assert(resourcePtr >= heapProperties.minResourceHeapReservedRange);

  imageMemoryManager.Deallocate(resourcePtr, heapProperties.imageDescriptorSize,
                                imageCursor);
}

auto DescriptorHeapManager::FreeBufferResourceIndex(VkDeviceSize resourcePtr)
    -> void {
  resourcePtr *= heapProperties.bufferDescriptorSize;
  resourcePtr = BufferCursorOrigin - resourcePtr;

  // Cannot free outside of used range
  assert(resourcePtr < bufferCursor);

  bufferMemoryManager.Deallocate(
      resourcePtr, heapProperties.bufferDescriptorSize, bufferCursor);
}

auto DescriptorHeapManager::FreeSamplerResourceIndex(VkDeviceSize resourcePtr)
    -> void {
  assert(resourcePtr <= samplerCursor); // Cannot free outside of used range
  assert(resourcePtr >= heapProperties.minSamplerHeapReservedRange);

  samplerMemoryManager.Deallocate(
      resourcePtr, heapProperties.samplerDescriptorSize, samplerCursor);
}

auto DescriptorHeapManager::Bind(VkCommandBuffer commandBuffer) -> Error {
  VkBindHeapInfoEXT samplerBindInfo{
      .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
      .pNext = nullptr,
      .heapRange =
          {
              .address = samplerAddress,
              .size = SamplerHeap->size,
          },
      .reservedRangeOffset = 0,
      .reservedRangeSize = heapProperties.minSamplerHeapReservedRange,
  };

  vkCmdBindSamplerHeapEXT(commandBuffer, &samplerBindInfo);

  VkBindHeapInfoEXT resourceBindInfo{
      .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
      .pNext = nullptr,
      .heapRange =
          {
              .address = resourceAddress,
              .size = ResourceHeap->size,
          },
      .reservedRangeOffset = 0,
      .reservedRangeSize = heapProperties.minResourceHeapReservedRange,
  };

  vkCmdBindResourceHeapEXT(commandBuffer, &resourceBindInfo);

  return {};
}

} // namespace Graphics