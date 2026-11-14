#include "recordingState.hpp"

namespace Graphics::RecordingState {

// NOLINTBEGIN
thread_local CombinedState CurrentState{};

thread_local CombinedState LastStateStorage{};
thread_local CombinedState *LastState{};

// If the last state was compute, we might misidentify that the scissor state was updated correctly
thread_local CombinedState LastGraphicsStateStorage{};
thread_local CombinedState *LastGraphicsState{};
// NOLINTEND

} // namespace Graphics::RecordingState