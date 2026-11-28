#include "allocator.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <vulkan/vulkan_core.h>

namespace Graphics {

PoolAllocator::PoolAllocator(VkDeviceSize size, VkDeviceSize chunkSize, VkDeviceSize alignment) : chunkSize(chunkSize), AllocatorInterface(size, alignment) {
  assert(chunkSize % alignment == 0UL);
  assert(backingAllocationSize % chunkSize == 0UL);

  chunksFree.resize(backingAllocationSize / chunkSize, true);
  allocationSizes.resize(backingAllocationSize / chunkSize, 0UL);
}
LinearAllocator::LinearAllocator(VkDeviceSize size, VkDeviceSize alignment) : AllocatorInterface(size, alignment) {}
auto LinearAllocator::Allocate(VkDeviceSize size) -> Result<VkDeviceSize> {
  ERR_ASSERT(pointer + size <= backingAllocationSize);

  VkDeviceSize address = pointer;
  pointer = Utils::AlignUp(pointer + size, memoryAlignment);

  return address;
}
auto LinearAllocator::DeAllocate(VkDeviceSize offset) -> Error { return Error::Create("Deallocation is not supported by the linear allocator"); }
auto LinearAllocator::Defragment() -> Error { return Error::Create("Defragmentation is not supported by the linear allocator"); }
auto StackAllocator::Allocate(VkDeviceSize size) -> Result<VkDeviceSize> {
  ERR_ASSERT(pointer + size <= backingAllocationSize);

  VkDeviceSize address = pointer;
  pointer = Utils::AlignUp(pointer + size, memoryAlignment);

  allocationAddresses.emplace_back(address);

  return address;
}
auto StackAllocator::DeAllocate(VkDeviceSize offset) -> Error {
  ERR_ASSERT_MSG(allocationAddresses.back() == offset, "The Stack allocator can only deallocate the last element");

  pointer = offset;
  allocationAddresses.pop_back();

  return {};
}
auto StackAllocator::Defragment() -> Error { return Error::Create("Defragmentation is not supported by the Stack allocator"); }
auto PoolAllocator::Allocate(VkDeviceSize size) -> Result<VkDeviceSize> {
  if (size > backingAllocationSize) {
    return Error::Create("Failed to allocate memory");
  }

  auto infoOpt = FindSpace(size);
  if (!infoOpt.has_value()) {
    return Error::Create("Failed to allocate memory");
  }

  auto info = infoOpt.value();

  for (uint32_t idx = 0UL; idx < info.second; idx++) {
    uint32_t chunkIdx = idx + info.first;

    ERR_ASSERT_MSG(chunksFree.at(chunkIdx), "Failed to allocate into a free chunk");
    ERR_ASSERT_MSG(allocationSizes.at(chunkIdx) == 0UL, "Failed to allocate into a free chunk");

    chunksFree.at(chunkIdx) = false;
    allocationSizes.at(chunkIdx) = idx == 0UL ? size : UINT64_MAX;
  }

  return info.first * chunkSize;
}
auto PoolAllocator::DeAllocate(VkDeviceSize offset) -> Error {
  ERR_ASSERT(offset % chunkSize == 0UL);

  uint32_t chunkIdx = offset / chunkSize;
  ERR_ASSERT_MSG(chunkIdx < allocationSizes.size(), "Deallocation out-of-range");

  VkDeviceSize size = allocationSizes.at(chunkIdx);
  ERR_ASSERT_MSG(size != 0UL || !chunksFree.at(chunkIdx), "Chunk was not allocated");

  VkDeviceSize chunkCount = Utils::CeilDiv(size, chunkSize);
  for (uint32_t idx = 0UL; idx < chunkCount; idx++) {
    // All blocks should be in use
    ERR_ASSERT_MSG(!chunksFree.at(idx + chunkIdx), "Memory range invalid");

    // All blocks except the root node should be marked as "used"
    if (idx != 0UL) {
      ERR_ASSERT_MSG(allocationSizes.at(idx + chunkIdx) == UINT64_MAX, "Memory range invalid");
    }
  }

  // Validated memory range, now we finalise the deallocation
  for (uint32_t idx = 0UL; idx < chunkCount; idx++) {
    chunksFree.at(idx + chunkIdx) = true;
    allocationSizes.at(idx + chunkIdx) = 0UL;
  }

  return {};
}
auto PoolAllocator::Defragment() -> Error {
  // Though it could be supported, I don't really wanna do that right now

  // Either we just move all memory to the left, filling holes (slow, lot's of moving)
  // Or we loop, starting from the right most element and iterate over all spaces to find holes
  // (excluding the current idx), moving it there if possible, I guess

  return Error::Create("Defragmentation is not supported by the Pool allocator");
}
auto PoolAllocator::FindSpace(VkDeviceSize size) -> std::optional<std::pair<uint32_t, uint32_t>> {
  if (size > backingAllocationSize) {
    return std::nullopt;
  }

  uint32_t chunksRequired = Utils::CeilDiv(size, chunkSize);
  uint32_t rootIdx = UINT32_MAX;
  uint32_t chunkCount = 0;

  if (chunksRequired == 0) {
    return std::nullopt;
  }

  for (uint32_t idx = 0; idx < chunksFree.size(); idx++) {
    const bool isFree = chunksFree.at(idx);

    if (isFree && rootIdx == UINT32_MAX) {
      rootIdx = idx;
    } else if (!isFree) {
      rootIdx = UINT32_MAX;
      chunkCount = 0;
    }

    chunkCount += static_cast<uint32_t>(isFree);

    if (chunksRequired == chunkCount) {
      return std::pair<uint32_t, uint32_t>{rootIdx, chunkCount};
    }
  }

  return std::nullopt;
}
StackAllocator::StackAllocator(VkDeviceSize size, VkDeviceSize alignment) : AllocatorInterface(size, alignment) {}

AllocatorInterface::AllocatorInterface(VkDeviceSize size, VkDeviceSize alignment) : memoryAlignment(alignment), backingAllocationSize(size) {};
AllocatorInterface::~AllocatorInterface() = default;
auto AllocatorInterface::Allocate(VkDeviceSize size) -> Result<VkDeviceSize> { return {}; }
auto AllocatorInterface::Allocate() -> Result<VkDeviceSize> { return {}; }
auto AllocatorInterface::DeAllocate(VkDeviceSize offset) -> Error { return {}; }
auto AllocatorInterface::Defragment() -> Error { return {}; }
auto AllocatorInterface::HasSpaceFor(VkDeviceSize size) -> bool { return {}; }
auto AllocatorInterface::IsEmpty() -> bool { return {}; }
auto AllocatorInterface::Resize(VkDeviceSize newSize) -> Error { return {}; }

auto PoolAllocator::HasSpaceFor(VkDeviceSize size) -> bool { return FindSpace(size).has_value(); }
auto PoolAllocator::IsEmpty() -> bool {
  return std::ranges::all_of(allocationSizes, [](const VkDeviceSize item) -> bool { return item == 0UL; });
}
auto PoolAllocator::Allocate() -> Result<VkDeviceSize> { return Error::Unexpected("Cannot allocate without size"); }
auto StackAllocator::Allocate() -> Result<VkDeviceSize> { return Error::Unexpected("Cannot allocate without size"); }
auto LinearAllocator::Allocate() -> Result<VkDeviceSize> { return Error::Unexpected("Cannot allocate without size"); }
auto GapAllocator::Allocate(VkDeviceSize size) -> Result<VkDeviceSize> { return Error::Unexpected("Gap allocator only works on resources with one size"); }

GapAllocator::GapAllocator(VkDeviceSize size, VkDeviceSize itemSize) : AllocatorInterface(size, 0), itemSize(itemSize) {}
auto GapAllocator::Allocate() -> Result<VkDeviceSize> {
  if (!gaps.empty()) {
    auto address = gaps.back();
    gaps.pop_back();
    return address;
  }

  ERR_ASSERT(pointer + itemSize <= backingAllocationSize);

  auto address = pointer;
  pointer += itemSize;

  return address;
}
auto GapAllocator::DeAllocate(VkDeviceSize offset) -> Error {
  // Technically offset could be uint64_max - size.. :p
  if (offset < pointer && offset == pointer - itemSize && !gaps.empty()) {
    pointer = offset;

    while (pointer - itemSize == gaps.back()) {
      gaps.pop_back();
      pointer -= itemSize;
    }

    return {};
  }

  gaps.insert(std::ranges::upper_bound(gaps, offset), offset);

  return {};
}
auto GapAllocator::Defragment() -> Error { return Error::Create("Defragmentation is not supported by the linear allocator"); }
auto GapAllocator::HasSpaceFor(VkDeviceSize size) -> bool {
  assert(size % itemSize == 0);
  VkDeviceSize count = size / itemSize;

  if (count < gaps.size()) {
    return true;
  }

  count -= gaps.size();

  return pointer + (count * itemSize) <= backingAllocationSize;
}

auto GapAllocator::IsEmpty() -> bool { return pointer == 0; }

auto PoolAllocator::Resize(VkDeviceSize size) -> Error {
  ERR_ASSERT(size % chunkSize == 0);

  chunksFree.resize(size / chunkSize, true);
  allocationSizes.resize(size / chunkSize, 0UL);
  backingAllocationSize = size;

  return {};
}

auto StackAllocator::Resize(VkDeviceSize size) -> Error {
  ERR_ASSERT(pointer <= size);

  backingAllocationSize = size;

  return {};
}

auto LinearAllocator::Resize(VkDeviceSize size) -> Error {
  ERR_ASSERT(pointer <= size);

  backingAllocationSize = size;

  return {};
}

auto GapAllocator::Resize(VkDeviceSize size) -> Error {
  ERR_ASSERT(pointer <= size);
  ERR_ASSERT(size % itemSize == 0);

  backingAllocationSize = size;

  return {};
}

} // namespace Graphics