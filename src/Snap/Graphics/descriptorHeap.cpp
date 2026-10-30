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
#include <mutex>
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
    .stagingBuffer = true,
    .persistentMapping = false,
    .debugName = "Resource heap",
  }));

  SamplerHeap = CHECK_RES(Buffer::Create(context, {
    .size = std::min(samplerHeapSize, maxSamplerHeapSize),
    .usage = VK_BUFFER_USAGE_2_DESCRIPTOR_HEAP_BIT_EXT | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
    .stagingBuffer = true,
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

  bufferDescriptorSize =
      Utils::AlignUp(heapProperties.bufferDescriptorSize,
                     heapProperties.bufferDescriptorAlignment);
  imageDescriptorSize = Utils::AlignUp(heapProperties.imageDescriptorSize,
                                       heapProperties.imageDescriptorAlignment);
  samplerDescriptorSize =
      Utils::AlignUp(heapProperties.samplerDescriptorSize,
                     heapProperties.samplerDescriptorAlignment);

  imageCursor += imageDescriptorSize;
  samplerCursor += samplerDescriptorSize;

  InvalidImageOffsets = {
      0,
      heapProperties.minResourceHeapReservedRange,
  };

  InvalidBufferOffsets = {
      0,
      heapProperties.minResourceHeapReservedRange,
  };

  InvalidSamplerOffsets = {
      0,
      heapProperties.minSamplerHeapReservedRange,
  };

  return {};
}

auto DescriptorHeapManager::DeInitialize() -> void {
  ResourceHeap.reset();
  SamplerHeap.reset();
}

auto DescriptorHeapManager::AllocateResource(bool isImage)
    -> Result<VkDeviceSize> {
  auto *memoryManager = isImage ? &imageMemoryManager : &bufferMemoryManager;
  VkDeviceSize size = isImage ? imageDescriptorSize : bufferDescriptorSize;

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

  ERR_ASSERT_MSG(imageCursor + imageDescriptorSize <
                     BufferCursorOrigin - (bufferCursor + bufferDescriptorSize),
                 "Out of heap memory");

  assert(!std::ranges::contains(
      isImage ? InvalidImageOffsets : InvalidBufferOffsets, cursor));

  return offset;
}

auto DescriptorHeapManager::AllocateSampler() -> Result<VkDeviceSize> {
  VkDeviceSize size = samplerDescriptorSize;
  auto addressFromGap =
      CHECK_RES(samplerMemoryManager.TryAllocateFromGap(size));

  if (addressFromGap.has_value()) {
    return addressFromGap.value();
  }

  VkDeviceSize offset = samplerCursor;
  ERR_ASSERT_MSG(offset + size < SamplerHeap->size, "Out of heap memory");

  // Same alignment argument as Resource allocation.
  samplerCursor = samplerCursor + size;

  assert(!std::ranges::contains(InvalidSamplerOffsets, samplerCursor));

  return offset;
}

auto DescriptorHeapManager::IsBufferOffsetValid(uint32_t offset) -> bool {
  return !std::ranges::contains(InvalidBufferOffsets,
                                offset * bufferDescriptorSize) &&
         (BufferCursorOrigin - (offset * bufferDescriptorSize)) < bufferCursor;
}

auto DescriptorHeapManager::IsImageOffsetValid(uint32_t offset) -> bool {
  return !std::ranges::contains(InvalidImageOffsets,
                                offset * imageDescriptorSize) &&
         (offset * imageDescriptorSize) < imageCursor;
}

auto DescriptorHeapManager::IsSamplerOffsetValid(uint32_t offset) -> bool {
  return !std::ranges::contains(InvalidSamplerOffsets,
                                offset * samplerDescriptorSize) &&
         (offset * samplerDescriptorSize) < samplerCursor;
}

auto DescriptorHeapManager::AllocateResource(const GraphicsContext &context,
                                             const Buffer *buffer,
                                             VkDescriptorType type,
                                             uint32_t offset, uint32_t size)
    -> Result<VkDeviceSize> {
  auto deviceAddress = CHECK_RES(buffer->GetDeviceAddress(context));

  return AllocateResource(context, deviceAddress, type, offset, size);
}

auto DescriptorHeapManager::AllocateResource(
    const GraphicsContext &context,
    VkAccelerationStructureKHR accelerationStructure) -> Result<VkDeviceSize> {

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = accelerationStructure,
  };

  VkDeviceAddress deviceAddress{};

  {
    std::lock_guard<std::mutex> lock(GraphicsContext::mutexes.device);

    deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(context.device,
                                                               &addressInfo);
  }

  return AllocateResource(context, deviceAddress,
                          VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 0, 0);
}

auto DescriptorHeapManager::AllocateResource(const GraphicsContext &context,
                                             VkDeviceAddress deviceAddress,
                                             VkDescriptorType type,
                                             uint32_t offset, uint32_t size)
    -> Result<VkDeviceSize> {

  VkDeviceAddressRangeKHR addressRange{
      .address = deviceAddress + offset,
      .size = size,
  };

  VkResourceDescriptorInfoEXT resourceInfo{
      .sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
      .pNext = nullptr,
      .type = type,
      .data = {.pAddressRange = &addressRange}};

  VkHostAddressRangeEXT hostAddressRange{};
  VkDeviceSize address{};
  std::vector<uint8_t> descriptor;
  descriptor.resize(bufferDescriptorSize);
  hostAddressRange.address = descriptor.data();
  hostAddressRange.size = descriptor.size();

  {
    // TODO: Own a lock for protecting allocate instead of relying on the device lock
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    address = CHECK_RES(AllocateResource(false));

    assert(bufferDescriptorSize <= 1024);

    CHECK_NEW_ERR(vkWriteResourceDescriptorsEXT(
        context.device, 1, &resourceInfo, &hostAddressRange));
  }

  CHECK_ERR(ResourceHeap->SetData(context, descriptor, address));

  return address / bufferDescriptorSize;
}

auto DescriptorHeapManager::AllocateResource(const GraphicsContext &context,
                                             const Texture *texture,
                                             VkDescriptorType type)
    -> Result<VkDeviceSize> {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

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

  assert(heapProperties.imageDescriptorSize <= 1024);
  std::vector<uint8_t> descriptor;
  descriptor.resize(imageDescriptorSize);

  hostAddressRange.address = descriptor.data();
  hostAddressRange.size = descriptor.size();

  CHECK_NEW_ERR(vkWriteResourceDescriptorsEXT(context.device, 1, &resourceInfo,
                                              &hostAddressRange));
  address = CHECK_RES(AllocateResource(true));
  CHECK_ERR(ResourceHeap->SetData(context, descriptor, address));

  return address / imageDescriptorSize;
}

auto DescriptorHeapManager::AllocateSampler(
    const GraphicsContext &context, VkSamplerCreateInfo samplerCreateInfo)
    -> Result<VkDeviceSize> {

  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  VkHostAddressRangeEXT hostAddressRange{};
  std::vector<uint8_t> descriptor;
  descriptor.resize(samplerDescriptorSize);

  hostAddressRange.address = descriptor.data();
  hostAddressRange.size = descriptor.size();

  CHECK_NEW_ERR(vkWriteSamplerDescriptorsEXT(
      context.device, 1, &samplerCreateInfo, &hostAddressRange));

  VkDeviceSize address = CHECK_RES(AllocateSampler());
  CHECK_ERR(SamplerHeap->SetData(context, descriptor, address));

  return address / samplerDescriptorSize;
}

auto DescriptorHeapManager::FreeImageResourceIndex(VkDeviceSize resourcePtr)
    -> void {
  resourcePtr *= imageDescriptorSize;

  assert(resourcePtr <= imageCursor); // Cannot free outside of used range
  assert(resourcePtr >= heapProperties.minResourceHeapReservedRange);

  imageMemoryManager.Deallocate(resourcePtr, imageDescriptorSize, imageCursor);
}

auto DescriptorHeapManager::FreeBufferResourceIndex(VkDeviceSize resourcePtr)
    -> void {
  resourcePtr *= bufferDescriptorSize;
  resourcePtr = BufferCursorOrigin - resourcePtr;

  // Cannot free outside of used range
  assert(resourcePtr < bufferCursor);

  bufferMemoryManager.Deallocate(resourcePtr, bufferDescriptorSize,
                                 bufferCursor);
}

auto DescriptorHeapManager::FreeSamplerResourceIndex(VkDeviceSize resourcePtr)
    -> void {
  resourcePtr *= samplerDescriptorSize;

  assert(resourcePtr <= samplerCursor); // Cannot free outside of used range
  assert(resourcePtr >= heapProperties.minSamplerHeapReservedRange);

  samplerMemoryManager.Deallocate(resourcePtr, samplerDescriptorSize,
                                  samplerCursor);
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