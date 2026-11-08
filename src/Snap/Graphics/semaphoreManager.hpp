#pragma once

#include "Modules/error.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {

/*
Semaphores are signalled every submit, so every vsync.
Every time a command buffer is acquired (async) We call NewSemaphoreValue
This increments the current timeline value and adds it to the pending map,
Each value in this map corresponds to a not-yet submitted command buffer
The uncompletedTimelineValues correspond to submitted not yet completed command buffers
We expect at least one command buffer to always be submitted, meaning
currentCPUTimelineValue will be incremented by AT LEAST 1 every vsync, but may be more.

when the user submits their command buffers in a vector, we consider them "sorted"
each of these have a unique timeline value, but are not in order.
*/

struct SemaphoreManager {
  VkSemaphore semaphore;
  uint64_t gpuCompletedTimelineValue = 0;

  std::atomic<uint64_t> currentCPUTimelineValue;
  std::condition_variable timelineCompletionCV;
  std::mutex timelineCompletionMutex;

  std::shared_mutex timelineSetsMutex;

  std::unordered_set<uint64_t> uncompletedTimelineValues;
  std::vector<uint64_t> sortedUncompletedTimelineValues;

  // [completed frame value] -> [original values that were remapped to this value]
  std::vector<std::pair<uint64_t, std::vector<uint64_t>>> uncompletedFrames;

  static auto GetSemaphoreValue() -> uint64_t;
  auto GetCompletedSemaphoreValue() const -> uint64_t;
  auto NewSemaphoreValue() -> uint64_t;

  auto IsInUse(uint64_t value) -> bool;

  // Set the pending timeline semaphore values
  // This will overwrite the existing set, this is to reorder the values
  // When command buffers are reordered before submission
  auto QueueTimelineValues(const std::vector<uint64_t> &values) -> void;

  auto UpdateSemaphoreValues(const struct GraphicsContext &context) -> Result<uint64_t>;
  auto Initialize(struct GraphicsContext &context) -> Error;
  auto Deinitialize(struct GraphicsContext &context) -> void;
};

} // namespace Graphics