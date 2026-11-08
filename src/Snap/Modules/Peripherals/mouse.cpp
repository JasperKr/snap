#include "mouse.hpp"
#include "Modules/error.hpp"
#include "SDL3/SDL_mouse.h"
#include <cstdint>

namespace Mouse {

auto GetX() -> float {
  float x_position = 0.0F;
  SDL_GetMouseState(&x_position, nullptr);
  return x_position;
}

auto GetY() -> float {
  float y_position = 0.0F;
  SDL_GetMouseState(nullptr, &y_position);
  return y_position;
}

auto GetPosition() -> Math::Vec2 {
  float x_position = 0.0F;
  float y_position = 0.0F;
  SDL_GetMouseState(&x_position, &y_position);
  return Math::Vec2{x_position, y_position};
}

auto SetPosition(float x_position, float y_position) -> void { SDL_WarpMouseInWindow(nullptr, x_position, y_position); }

auto SetPosition(const Math::Vec2 &position) -> void { SDL_WarpMouseInWindow(nullptr, position.x, position.y); }

auto IsButtonDown(const std::string &buttonName) -> bool {
  auto iterator = MouseButtonMap.find(buttonName);
  if (iterator == MouseButtonMap.end()) {
    return false;
  }

  uint32_t buttonState = SDL_GetMouseState(nullptr, nullptr);
  return (buttonState & iterator->second) != 0;
}

auto SetVisible(bool show) -> void {
  if (show) {
    SDL_ShowCursor();
  } else {
    SDL_HideCursor();
  }
}

auto IsCursorVisible() -> bool { return SDL_CursorVisible(); }

auto SetCursor(const Ref<MouseCursor> &cursor) -> Error {
  if (cursor->sdlCursor == nullptr) {
    return Error::Create("Mouse cursor is null.");
  }

  if (cursor->sdlCursor == SDL_GetCursor()) {
    return Error::Success();
  }

  bool success = SDL_SetCursor(cursor->sdlCursor);

  if (!success) {
    const auto *sdlError = SDL_GetError();
    return Error::Create("Failed to set mouse cursor: " + std::string(sdlError));
  }

  return Error::Success();
}

auto CreateSystemCursor(SDL_SystemCursor sdlCursorType) -> Result<Ref<MouseCursor>> {
  if (sdlCursorType == SDL_SYSTEM_CURSOR_COUNT) {
    return Error::Create("Unknown system cursor type.");
  }

  SDL_Cursor *sdlCursor = SDL_CreateSystemCursor(sdlCursorType);

  if (sdlCursor == nullptr) {
    const auto *sdlError = SDL_GetError();
    return Error::Create("Failed to create system mouse cursor: " + std::string(sdlError));
  }

  return Ref<MouseCursor>::Make(sdlCursor);
}

auto StringToSDLCursor(const std::string &cursorName) -> SDL_SystemCursor {
  if (cursorName == "arrow") {
    return SDL_SYSTEM_CURSOR_DEFAULT;
  }
  if (cursorName == "ibeam") {
    return SDL_SYSTEM_CURSOR_TEXT;
  }
  if (cursorName == "wait") {
    return SDL_SYSTEM_CURSOR_WAIT;
  }
  if (cursorName == "crosshair") {
    return SDL_SYSTEM_CURSOR_CROSSHAIR;
  }
  if (cursorName == "progress") {
    return SDL_SYSTEM_CURSOR_PROGRESS;
  }
  if (cursorName == "resizenwse") {
    return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
  }
  if (cursorName == "resizenesw") {
    return SDL_SYSTEM_CURSOR_NESW_RESIZE;
  }
  if (cursorName == "resizewe") {
    return SDL_SYSTEM_CURSOR_EW_RESIZE;
  }
  if (cursorName == "resizens") {
    return SDL_SYSTEM_CURSOR_NS_RESIZE;
  }
  if (cursorName == "move") {
    return SDL_SYSTEM_CURSOR_MOVE;
  }
  if (cursorName == "notallowed") {
    return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
  }
  if (cursorName == "pointer") {
    return SDL_SYSTEM_CURSOR_POINTER;
  }
  // Default fallback
  return SDL_SYSTEM_CURSOR_COUNT;
}
} // namespace Mouse