#include "recordingState.hpp"

namespace Graphics::RecordingState {

// NOLINTBEGIN
thread_local RenderState::State CurrentState{};
thread_local RenderState::State LastStateStorage{};
thread_local RenderState::State *LastState{};
// NOLINTEND

} // namespace Graphics::RecordingState