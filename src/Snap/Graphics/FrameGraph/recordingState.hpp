#pragma once

#include "Graphics/FrameGraph/virtualState.hpp"
namespace Graphics::RecordingState {

struct CombinedState {
  VirtualRenderState const *renderState{};

  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
  VkViewport viewport{};
  VkRect2D scissor{};
};

// NOLINTBEGIN
thread_local extern CombinedState CurrentState;
thread_local extern CombinedState LastStateStorage;
thread_local extern CombinedState *LastState;
// NOLINTEND

} // namespace Graphics::RecordingState