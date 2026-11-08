#include "wrap_mouse.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/Peripherals/mouse.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "SDL3/SDL_mouse.h"
#include "SDL3/SDL_surface.h"
#include "Wrap/wrap.hpp"
#include <cstdint>
#include <lua.hpp>
#include <map>
#include <string>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Wrap::Mouse {

// button: string|integer...
// Returns: boolean anyDown, boolean button1Down, boolean button2Down, ...
auto Wrap_IsDown(lua_State *state) -> int {
  int keyCount = lua_gettop(state);
  bool anyDown = false;
  thread_local std::vector<bool> mouseStates;
  mouseStates.clear();
  mouseStates.reserve(static_cast<size_t>(keyCount));
  auto mouseState = SDL_GetMouseState(nullptr, nullptr);

  for (int i = 1; i <= keyCount; ++i) {
    bool isString = lua_type(state, i) == LUA_TSTRING;
    uint32_t button = 0;

    if (isString) {
      const char *keyString = luaL_checkstring(state, i);
      auto iterator = ::Mouse::MouseButtonMap.find(std::string(keyString));
      if (iterator == ::Mouse::MouseButtonMap.end()) {
        luaL_error(state, "Unknown mouse button key: %s", keyString);
        return 0;
      }
      button = iterator->second;
    } else {
      button = static_cast<int>(luaL_checkinteger(state, i));
    }

    // NOLINTNEXTLINE
    bool isDown = (mouseState & SDL_BUTTON_MASK(button)) != 0U;
    if (isDown) {
      anyDown = true;
    }
    mouseStates.emplace_back(isDown);
  }

  lua_pushboolean(state, anyDown ? 1 : 0);
  for (bool keyState : mouseStates) {
    lua_pushboolean(state, keyState ? 1 : 0);
  }
  return 1 + keyCount;
}

auto Wrap_GetX(lua_State *state) -> int {
  float x_position = 0.0F;
  SDL_GetMouseState(&x_position, nullptr);
  lua_pushnumber(state, x_position);
  return 1;
}

auto Wrap_GetY(lua_State *state) -> int {
  float y_position = 0.0F;
  SDL_GetMouseState(nullptr, &y_position);
  lua_pushnumber(state, y_position);
  return 1;
}

auto Wrap_GetPosition(lua_State *state) -> int {
  float x_position = 0.0F;
  float y_position = 0.0F;
  SDL_GetMouseState(&x_position, &y_position);
  lua_pushnumber(state, x_position);
  lua_pushnumber(state, y_position);
  return 2;
}

auto Wrap_SetPosition(lua_State *state) -> int {
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  SDL_WarpMouseInWindow(nullptr, x_position, y_position);
  return 0;
}

auto Wrap_SetX(lua_State *state) -> int {
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  float y_position = 0.0F;
  SDL_GetMouseState(nullptr, &y_position);
  SDL_WarpMouseInWindow(nullptr, x_position, y_position);
  return 0;
}

auto Wrap_SetY(lua_State *state) -> int {
  float x_position = 0.0F;
  auto y_position = static_cast<float>(luaL_checknumber(state, 1));
  SDL_GetMouseState(&x_position, nullptr);
  SDL_WarpMouseInWindow(nullptr, x_position, y_position);
  return 0;
}

auto Wrap_SetRelativeMode(lua_State *state) -> int {
  auto enabled = static_cast<bool>(lua_toboolean(state, 1));
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  SDL_SetWindowRelativeMouseMode(ctx->sdlWindow, enabled);
  return 0;
}

auto Wrap_GetRelativeMode(lua_State *state) -> int {
  auto *ctx = Graphics::GetCurrentGraphicsContext();
  auto enabled = SDL_GetWindowRelativeMouseMode(ctx->sdlWindow);
  lua_pushboolean(state, static_cast<int>(enabled));
  return 1;
}

auto Wrap_SetVisible(lua_State *state) -> int {
  auto visible = static_cast<bool>(lua_toboolean(state, 1));
  if (visible) {
    SDL_ShowCursor();
  } else {
    SDL_HideCursor();
  }
  return 0;
}

auto Wrap_GetVisible(lua_State *state) -> int {
  auto visible = SDL_CursorVisible();
  lua_pushboolean(state, static_cast<int>(visible));
  return 1;
}

auto Wrap_GetHardwareCursor(lua_State *state) -> int {
  auto cursorName = std::string(luaL_checkstring(state, 1));
  SDL_SystemCursor sdlCursorType = ::Mouse::StringToSDLCursor(cursorName);
  if (sdlCursorType == SDL_SYSTEM_CURSOR_COUNT) {
    luaL_error(state, "Unknown cursor type: %s", cursorName.c_str());
    return 0;
  }

  SDL_Cursor *sdlCursor = SDL_CreateSystemCursor(sdlCursorType);

  if (sdlCursor == nullptr) {
    luaL_error(state, "Failed to create system cursor: %s", SDL_GetError());
    return 0;
  }

  auto mouseCursor = Ref<::Mouse::MouseCursor>::Make(sdlCursor);

  LuaWrap::PushObject(state, ::Mouse::MouseCursor::GetType(), mouseCursor.get());

  return 1;
}

auto Wrap_NewCursor(lua_State *state) -> int {
  auto data = LUA_CK_NULL(LuaWrap::ObjectFromLua<Image::ImageData>(state, 1));
  int hotX = static_cast<int>(luaL_checkinteger(state, 2));
  int hotY = static_cast<int>(luaL_checkinteger(state, 3));

  SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA32;

  LUA_ASSERT_MSG(data->GetFormat() == VK_FORMAT_R8G8B8A8_UNORM, "ImageData must be in rgba8 format");

  SDL_Surface *surface = SDL_CreateSurfaceFrom(static_cast<int32_t>(data->GetWidth()), static_cast<int32_t>(data->GetHeight()), format, data->GetDataPtr(),
                                               static_cast<int32_t>(data->GetFormatSize()));

  if (surface == nullptr) {
    luaL_error(state, "Failed to create surface for cursor: %s", SDL_GetError());
    return 0;
  }

  SDL_Cursor *sdlCursor = SDL_CreateColorCursor(surface, hotX, hotY);
  SDL_DestroySurface(surface);

  if (sdlCursor == nullptr) {
    luaL_error(state, "Failed to create color cursor: %s", SDL_GetError());
    return 0;
  }

  auto mouseCursor = Ref<::Mouse::MouseCursor>::Make(sdlCursor);

  LuaWrap::PushObject(state, ::Mouse::MouseCursor::GetType(), mouseCursor.get());

  return 1;
}

auto Wrap_SetCursor(lua_State *state) -> int {
  auto mouseCursor = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Mouse::MouseCursor>(state, 1));

  SDL_SetCursor(mouseCursor->sdlCursor);
  return 0;
}

} // namespace Wrap::Mouse