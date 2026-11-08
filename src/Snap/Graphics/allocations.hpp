#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {
// #define ENABLE_VULKAN_ALLOCATOR_TRACKING

struct AllocationStats {
  size_t totalAllocated = 0;
  size_t totalFreed = 0;
  size_t currentlyAllocated = 0;
  size_t totalAllocationCount = 0;
  size_t totalFreeCount = 0;
};

struct ThreadAllocations {
  VkAllocationCallbacks callbacks{};

  AllocationStats stats;

  std::unordered_map<void *, size_t> allocationSizes;

  static auto Allocate(void *userData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) -> void *;
  static auto Reallocate(void *userData, void *original, size_t size, size_t alignment, VkSystemAllocationScope allocationScope) -> void *;
  static void Free(void *userData, void *memory);

  auto Initialize() {
    callbacks.pfnAllocation = Allocate;
    callbacks.pfnReallocation = Reallocate;
    callbacks.pfnFree = Free;
    callbacks.pfnInternalAllocation = nullptr;
    callbacks.pfnInternalFree = nullptr;
    callbacks.pUserData = this;
  }
};

auto GetAllocationCallbacks() -> const VkAllocationCallbacks *;
auto GetThreadAllocations() -> ThreadAllocations &;

struct Allocations {
  std::vector<ThreadAllocations *> allocations;
  std::mutex allocationsMutex;

  void RegisterNewThreadAllocations() {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    GetThreadAllocations().Initialize();
    allocations.push_back(&GetThreadAllocations());
  }

  auto GetThreadStats() -> std::vector<AllocationStats> {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    std::vector<AllocationStats> stats;
    stats.clear();
    stats.reserve(allocations.size());
    for (const auto &threadAlloc : allocations) {
      stats.emplace_back(threadAlloc->stats);
    }
    return stats;
  }

  auto GetTotalStats() -> AllocationStats {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    AllocationStats totalStats;
    for (const auto &threadAlloc : allocations) {
      totalStats.totalAllocated += threadAlloc->stats.totalAllocated;
      totalStats.totalFreed += threadAlloc->stats.totalFreed;
      totalStats.currentlyAllocated += threadAlloc->stats.currentlyAllocated;
      totalStats.totalAllocationCount += threadAlloc->stats.totalAllocationCount;
      totalStats.totalFreeCount += threadAlloc->stats.totalFreeCount;
    }
    return totalStats;
  }
};

extern Allocations GlobalAllocations; // NOLINT

} // namespace Graphics