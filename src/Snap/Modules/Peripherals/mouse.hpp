#pragma once

#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "SDL3/SDL_mouse.h"
#include <cstdint>
#include <map>
#include <string>
namespace Mouse {

const std::map<std::string, uint32_t> MouseButtonMap = {
    {"left", SDL_BUTTON_LEFT}, {"middle", SDL_BUTTON_MIDDLE}, {"right", SDL_BUTTON_RIGHT}, {"x1", SDL_BUTTON_X1}, {"x2", SDL_BUTTON_X2},
};

const static Type LuaMouseCursorType = Type("MouseCursor");

struct MouseCursor : Object {
  MouseCursor(const MouseCursor &) = delete;
  MouseCursor(MouseCursor &&) = delete;
  auto operator=(const MouseCursor &) -> MouseCursor & = delete;
  auto operator=(MouseCursor &&) -> MouseCursor & = delete;
  explicit MouseCursor(SDL_Cursor *sdlCursor) : sdlCursor(sdlCursor) {}
  SDL_Cursor *sdlCursor;

  ~MouseCursor() override { SDL_DestroyCursor(sdlCursor); }

  [[nodiscard]] static auto GetType() -> Type const * { return &LuaMouseCursorType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override { return MouseCursor::GetType(); }
};

auto StringToSDLCursor(const std::string &cursorName) -> SDL_SystemCursor;
auto GetX() -> float;
auto GetY() -> float;
auto GetPosition() -> Math::Vec2;
auto SetPosition(float x_position, float y_position) -> void;
auto SetPosition(const Math::Vec2 &position) -> void;
auto IsButtonDown(const std::string &buttonName) -> bool;
auto SetVisible(bool show) -> void;
auto IsCursorVisible() -> bool;
auto SetCursor(const Ref<MouseCursor> &cursor) -> Error;
auto CreateSystemCursor(SDL_SystemCursor sdlCursorType) -> Result<Ref<MouseCursor>>;

} // namespace Mouse