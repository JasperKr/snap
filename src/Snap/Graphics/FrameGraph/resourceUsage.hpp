#pragma once

#include <vulkan/vulkan_core.h>
namespace Graphics {

struct ResourceState {
  VkPipelineStageFlags2 stages;
  VkAccessFlags2 access;
};

struct AccessState {
  mutable VkPipelineStageFlags2 lastWriteStageFlags = 0;
  mutable VkAccessFlags2 lastWriteAccessFlags = 0;

  mutable VkPipelineStageFlags2 lastReadStageFlags = 0;
  mutable VkAccessFlags2 lastReadAccessFlags = 0;

  mutable uint64_t lastWriteTimelineIndex = 0;
  mutable uint64_t lastReadTimelineIndex = 0;
};

} // namespace Graphics