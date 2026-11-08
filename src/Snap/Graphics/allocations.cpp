#include "allocations.hpp"
#include <cstring>

namespace Graphics {

auto ThreadAllocations::Allocate(void *userData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) -> void * {
  auto *allocations = static_cast<ThreadAllocations *>(userData);
  allocations->stats.totalAllocated += size;
  allocations->stats.currentlyAllocated += size;
  allocations->stats.totalAllocationCount++;

  auto *memory = ::operator new(size, std::align_val_t(alignment));

  allocations->allocationSizes[memory] = size;

  return memory;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto ThreadAllocations::Reallocate(void *userData, void *original, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) -> void * {
  auto *allocations = static_cast<ThreadAllocations *>(userData);

  if (original == nullptr) {
    return Allocate(userData, size, alignment, allocationScope);
  }

  if (size == 0) {
    Free(userData, original);
    return nullptr;
  }

  auto iter = allocations->allocationSizes.find(original);
  if (iter == allocations->allocationSizes.end()) {
    return nullptr; // Original pointer not found in allocation map
  }

  size_t originalSize = iter->second;
  void *newMemory = Allocate(userData, size, alignment, allocationScope);
  if (newMemory != nullptr) {
    std::memcpy(newMemory, original, std::min(originalSize, size));
    Free(userData, original);
  }

  return newMemory;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ThreadAllocations::Free(void *userData, void *memory) {
  auto *allocations = static_cast<ThreadAllocations *>(userData);
  auto iter = allocations->allocationSizes.find(memory);
  if (iter != allocations->allocationSizes.end()) {
    size_t size = iter->second;
    allocations->stats.totalFreed += size;
    allocations->stats.currentlyAllocated -= size;
    allocations->stats.totalFreeCount++;
    allocations->allocationSizes.erase(iter);
  }
  ::operator delete(memory, std::align_val_t(alignof(std::max_align_t)));
}

auto GetThreadAllocations() -> ThreadAllocations & {
  thread_local ThreadAllocations allocations;
  return allocations;
}

#ifdef ENABLE_VULKAN_ALLOCATOR_TRACKING
auto GetAllocationCallbacks() -> const VkAllocationCallbacks * { return &GetThreadAllocations().callbacks; }
#else
auto GetAllocationCallbacks() -> const VkAllocationCallbacks * {
  return nullptr; // Use default Vulkan allocator
}
#endif

Allocations GlobalAllocations{}; // NOLINT

} // namespace Graphics