#include "Graphics/semaphoreManager.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"

#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "vulkan/vulkan_core.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <public/tracy/Tracy.hpp>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace Graphics {

auto SemaphoreManager::GetSemaphoreValue() -> uint64_t {
  auto value = GetThreadContext().timelineValue;
  assert(value != 0);
  return value;
}

auto SemaphoreManager::GetCompletedSemaphoreValue() const -> uint64_t {
  return gpuCompletedTimelineValue;
}

auto SemaphoreManager::NewSemaphoreValue() -> uint64_t {
  // Add 1 to allow for easy invalid value check
  auto value = currentCPUTimelineValue.fetch_add(1) + 1;

  {
    std::unique_lock lock(timelineSetsMutex);
    uncompletedTimelineValues.emplace(value);
  }
  return value;
}

auto SemaphoreManager::IsInUse(uint64_t value) -> bool {
  std::shared_lock lock(timelineSetsMutex);
  return uncompletedTimelineValues.contains(value);
}

auto SemaphoreManager::QueueTimelineValues(const std::vector<uint64_t> &values)
    -> void {
  std::unique_lock lock(timelineSetsMutex);
  sortedUncompletedTimelineValues.insert(sortedUncompletedTimelineValues.end(),
                                         values.begin(), values.end());
}

// Update the semaphore values before submitting command buffers
// And returns the latest queued timeline value for signalling with a semaphore
auto SemaphoreManager::UpdateSemaphoreValues(const GraphicsContext &context)
    -> Result<uint64_t> {
  ZoneScoped;

  std::lock_guard lock(timelineSetsMutex);

  // currentFrame starts at 0, but vulkan complains when signalling a timeline semaphore with value 0, so we start at 1
  uint64_t frameIdx = context.currentFrame + 1;
  uint64_t maxValue = GetSemaphoreValue();

  uncompletedFrames.emplace_back(frameIdx, sortedUncompletedTimelineValues);
  sortedUncompletedTimelineValues.clear();

  {
    std::unique_lock<std::mutex> lock(
        Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(vkGetSemaphoreCounterValue(context.device, semaphore,
                                             &gpuCompletedTimelineValue));
  }

  // Remove completed timeline values
  Utils::UnorderedErase(uncompletedFrames, [&](const auto &value) -> bool {
    bool erase = value.first < gpuCompletedTimelineValue;

    if (erase) {
      for (const auto &originalValue : value.second) {
        uncompletedTimelineValues.erase(originalValue);
      }
    }

    return erase;
  });

  {
    std::lock_guard lock(timelineCompletionMutex);
    timelineCompletionCV.notify_all();
  }

  return frameIdx;
}

auto SemaphoreManager::Initialize(GraphicsContext &context) -> Error {
  VkSemaphoreTypeCreateInfo timelineInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
  };

  VkSemaphoreCreateInfo semInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineInfo,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateSemaphore(context.device, &semInfo,
                                    GetAllocationCallbacks(), &semaphore));
  }

  return Error::Success();
}

auto SemaphoreManager::Deinitialize(GraphicsContext &context) -> void {
  if (semaphore != VK_NULL_HANDLE) {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkDestroySemaphore(context.device, semaphore, GetAllocationCallbacks());
    semaphore = VK_NULL_HANDLE;
  }
}

} // namespace Graphics