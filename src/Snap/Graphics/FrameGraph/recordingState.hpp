#pragma once

#include "Graphics/renderState.hpp"
namespace Graphics::RecordingState {

// NOLINTBEGIN
thread_local extern RenderState::State CurrentState;
thread_local extern RenderState::State LastStateStorage;
thread_local extern RenderState::State *LastState;
// NOLINTEND

} // namespace Graphics::RecordingState