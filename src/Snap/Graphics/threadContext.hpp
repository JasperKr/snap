#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Graphics/graphicsState.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {
struct DescriptorPoolInfo {
  VkDescriptorPool descriptorPool;
  uint64_t lastUsedTimestamp;
};

struct VirtualCommandBuffer;

// Per-thread context for graphics operations, even on the main thread
struct ThreadContext {
  struct GraphicsContext *graphicsContext = nullptr; // Global graphics context
  VkCommandPool commandPool = VK_NULL_HANDLE;        // Per-thread command pool
  std::shared_ptr<VirtualCommandBuffer> commandBuffer; // Current command buffer
  VkCommandBuffer workingCommandBuffer;

  // unique identifier to not the frame, but recording of command buffer
  uint64_t recordingIdentifier = 0;
  ObjectID currentMesh;

  VkBuffer boundIndexBuffer;
  Math::StackVector<VkBuffer, MAX_BOUND_VERTEX_BUFFERS> boundVertexBuffers;

  std::vector<DescriptorPoolInfo> descriptorPools;  // Descriptor pool info
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE; // Current descriptor pool

  uint64_t timelineValue = 0;

  uint32_t queueFamily;
  VkQueueFlags queueFlags;
};
} // namespace Graphics