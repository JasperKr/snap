#include "recordingState.hpp"

namespace Graphics::RecordingState {

// NOLINTBEGIN
thread_local CombinedState CurrentState{};
thread_local CombinedState LastStateStorage{};
thread_local CombinedState *LastState{};
// NOLINTEND

} // namespace Graphics::RecordingState