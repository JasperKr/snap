#pragma once

#include <cstdint>

#include "vulkan/vulkan_core.h"

namespace Graphics {
struct VirtualCommandBuffer;

// Per-thread context for graphics operations, even on the main thread
struct ThreadContext {
  struct GraphicsContext *graphicsContext = nullptr; // Global graphics context
  VkCommandPool commandPool = VK_NULL_HANDLE;        // Per-thread command pool
  VirtualCommandBuffer *commandBuffer{};             // Current command buffer
  VkCommandBuffer workingCommandBuffer{};

  // unique identifier to not the frame, but recording of command buffer
  uint64_t recordingIdentifier = 0;

  uint64_t timelineValue = 0;

  uint32_t queueFamily{};
  VkQueueFlags queueFlags{};
};
} // namespace Graphics