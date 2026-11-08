#include "wrap_window.hpp"
#include "Modules/error.hpp"
#include "Modules/window.hpp"

#include <lua.hpp>
namespace Wrap::Window {

auto wrap_Hide(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  ::Window::Hide(window);
  return 0;
}

auto wrap_GetDisplayCount(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int displayCount = ::Window::GetDisplayCount(window);
  lua_pushinteger(state, displayCount);
  return 1;
}
auto wrap_GetDisplayName(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int displayIndex = static_cast<int>(luaL_checkinteger(state, 1));
  std::string name = ::Window::GetDisplayName(window, displayIndex);
  lua_pushstring(state, name.c_str());
  return 1;
}
auto wrap_IsFullscreen(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool isFullscreen = ::Window::IsFullscreen(window);
  lua_pushboolean(state, static_cast<int>(isFullscreen));
  return 1;
}
auto wrap_GetFullscreenDimensions(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int displayIndex = static_cast<int>(luaL_checkinteger(state, 1));
  std::vector<Math::Uvec2> resolutions = ::Window::GetFullscreenDimensions(window, displayIndex);

  lua_newtable(state);
  for (size_t i = 0; i < resolutions.size(); ++i) {
    lua_newtable(state);

    lua_pushinteger(state, static_cast<lua_Integer>(resolutions[i].x));
    lua_setfield(state, -2, "width");

    lua_pushinteger(state, static_cast<lua_Integer>(resolutions[i].y));
    lua_setfield(state, -2, "height");

    lua_rawseti(state, -2, static_cast<int>(i) + 1);
  }

  return 1;
}
auto wrap_SetFullscreen(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool fullscreen = lua_toboolean(state, 1) != 0;
  ::Window::SetFullscreen(window, fullscreen);
  return 0;
}
auto wrap_GetWidth(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int width = ::Window::GetWidth(window);
  lua_pushinteger(state, static_cast<lua_Integer>(width));
  return 1;
}
auto wrap_GetHeight(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int height = ::Window::GetHeight(window);
  lua_pushinteger(state, static_cast<lua_Integer>(height));
  return 1;
}
auto wrap_GetDimensions(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  Math::Uvec2 dimensions = ::Window::GetDimensions(window);

  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.x));
  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.y));

  return 2;
}
auto wrap_SetDimensions(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int width = static_cast<int>(luaL_checkinteger(state, 1));
  int height = static_cast<int>(luaL_checkinteger(state, 2));
  ::Window::SetDimensions(window, width, height);
  return 0;
}
auto wrap_SetIcon(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  Ref<Image::ImageData> icon = *static_cast<Ref<Image::ImageData> *>(lua_touserdata(state, 1));
  ::Window::SetIcon(window, icon);
  return 0;
}

inline auto FullscreenModeToString(::Window::FullscreenMode mode) -> const char * {
  switch (mode) {
  case ::Window::FullscreenMode::Windowed:
    return "windowed";
  case ::Window::FullscreenMode::Fullscreen:
    return "fullscreen";
  case ::Window::FullscreenMode::Borderless:
    return "borderless";
  default:
    return "unknown";
  }
}

inline auto VsyncModeToString(::Window::VsyncMode mode) -> const char * {
  switch (mode) {
  case ::Window::VsyncMode::Immediate:
    return "immediate";
  case ::Window::VsyncMode::Replace:
    return "replace";
  case ::Window::VsyncMode::Enabled:
    return "enabled";
  case ::Window::VsyncMode::Adaptive:
    return "adaptive";
  default:
    return "unknown";
  }
}

inline auto StringToFullscreenMode(const char *str) -> Result<::Window::FullscreenMode> {
  if (std::string(str) == "windowed") {
    return ::Window::FullscreenMode::Windowed;
  }
  if (std::string(str) == "fullscreen") {
    return ::Window::FullscreenMode::Fullscreen;
  }
  if (std::string(str) == "borderless") {
    return ::Window::FullscreenMode::Borderless;
  }

  return Error::Unexpectedf("Invalid fullscreen mode '{}'", str);
}

inline auto StringToVsyncMode(const char *str) -> Result<::Window::VsyncMode> {
  if (std::string(str) == "immediate") {
    return ::Window::VsyncMode::Immediate;
  }
  if (std::string(str) == "replace") {
    return ::Window::VsyncMode::Replace;
  }
  if (std::string(str) == "enabled") {
    return ::Window::VsyncMode::Enabled;
  }
  if (std::string(str) == "adaptive") {
    return ::Window::VsyncMode::Adaptive;
  }

  return Error::Unexpectedf("Invalid vsync mode '{}'", str);
}

inline auto ColorSpaceToString(::Window::ColorSpace colorSpace) -> const char * {
  switch (colorSpace) {
  case ::Window::ColorSpace::GammaCorrect:
    return "gammacorrect";
  case ::Window::ColorSpace::Linear:
    return "linear";
  case ::Window::ColorSpace::HDR:
    return "hdr";
  default:
    return "unknown";
  }
}

inline auto StringToColorSpace(const char *str) -> Result<::Window::ColorSpace> {
  if (std::string(str) == "gammacorrect") {
    return ::Window::ColorSpace::GammaCorrect;
  }
  if (std::string(str) == "linear") {
    return ::Window::ColorSpace::Linear;
  }
  if (std::string(str) == "hdr") {
    return ::Window::ColorSpace::HDR;
  }

  return Error::Unexpectedf("Invalid color space '{}'", str);
}

auto wrap_GetSettings(lua_State *state) -> int {
  auto *wcontext = ::Window::GetWindowContext();

  ::Window::Settings settings = ::Window::GetSettings(*wcontext);
  lua_newtable(state);

  lua_pushboolean(state, static_cast<int>(settings.resizable));
  lua_setfield(state, -2, "resizable");

  lua_pushboolean(state, static_cast<int>(settings.borderless));
  lua_setfield(state, -2, "borderless");

  lua_pushboolean(state, static_cast<int>(settings.fullscreen));
  lua_setfield(state, -2, "fullscreen");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.displayIndex));
  lua_setfield(state, -2, "displayIndex");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.width));
  lua_setfield(state, -2, "width");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.height));
  lua_setfield(state, -2, "height");

  const auto *vsyncMode = VsyncModeToString(settings.vsync);
  lua_pushstring(state, vsyncMode);
  lua_setfield(state, -2, "vsync");

  const auto *fullscreenMode = FullscreenModeToString(settings.fullscreenMode);
  lua_pushstring(state, fullscreenMode);
  lua_setfield(state, -2, "fullscreenMode");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.minimumWidth));
  lua_setfield(state, -2, "minimumWidth");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.minimumHeight));
  lua_setfield(state, -2, "minimumHeight");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.xPosition));
  lua_setfield(state, -2, "xPosition");

  lua_pushinteger(state, static_cast<lua_Integer>(settings.yPosition));
  lua_setfield(state, -2, "yPosition");

  const auto *colorSpace = ColorSpaceToString(settings.colorSpace);
  lua_pushstring(state, colorSpace);
  lua_setfield(state, -2, "colorspace");

  return 1;
}

inline auto SettingsFromStack(lua_State *state) -> Result<::Window::Settings> {
  ::Window::Settings settings;

  lua_getfield(state, 1, "resizable");
  if (!lua_isnoneornil(state, -1)) {
    settings.resizable = lua_toboolean(state, -1) != 0;
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "borderless");
  if (!lua_isnoneornil(state, -1)) {
    settings.borderless = lua_toboolean(state, -1) != 0;
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "fullscreen");
  if (!lua_isnoneornil(state, -1)) {
    settings.fullscreen = lua_toboolean(state, -1) != 0;
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "displayIndex");
  if (!lua_isnoneornil(state, -1)) {
    settings.displayIndex = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "width");
  if (!lua_isnoneornil(state, -1)) {
    settings.width = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "height");
  if (!lua_isnoneornil(state, -1)) {
    settings.height = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "vsync");
  if (!lua_isnoneornil(state, -1)) {
    settings.vsync = CHECK_RES(StringToVsyncMode(luaL_checkstring(state, -1)));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "fullscreenMode");
  if (!lua_isnoneornil(state, -1)) {
    auto fullscreenModeResult = StringToFullscreenMode(luaL_checkstring(state, -1));
    if (Error::IsError(fullscreenModeResult)) {
      return fullscreenModeResult.error();
    }
    settings.fullscreenMode = fullscreenModeResult.value();
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "minimumWidth");
  if (!lua_isnoneornil(state, -1)) {
    settings.minimumWidth = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "minimumHeight");
  if (!lua_isnoneornil(state, -1)) {
    settings.minimumHeight = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "xPosition");
  if (!lua_isnoneornil(state, -1)) {
    settings.xPosition = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "yPosition");
  if (!lua_isnoneornil(state, -1)) {
    settings.yPosition = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "colorspace");
  if (!lua_isnoneornil(state, -1)) {
    settings.colorSpace = CHECK_RES(StringToColorSpace(luaL_checkstring(state, -1)));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "title");
  if (!lua_isnoneornil(state, -1)) {
    if (lua_isstring(state, -1) == 0) {
      return Error::Unexpected("Title must be a string");
    }
    settings.title = lua_tostring(state, -1);
  }
  lua_pop(state, 1);

  return settings;
}

auto wrap_SetSettings(lua_State *state) -> int {

  auto *wcontext = ::Window::GetWindowContext();

  auto settingsResult = SettingsFromStack(state);
  if (Error::IsError(settingsResult)) {
    return luaL_error(state, "%s", settingsResult.error().message.c_str());
  }

  ::Window::SetSettings(*wcontext, settingsResult.value());
  return 0;
}

auto wrap_UpdateSettings(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  ::Window::SettingsUpdate update;

  lua_getfield(state, 1, "resizable");
  if (!lua_isnoneornil(state, -1)) {
    update.resizable = lua_toboolean(state, -1) != 0;
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "borderless");
  if (!lua_isnoneornil(state, -1)) {
    update.borderless = lua_toboolean(state, -1) != 0;
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "fullscreen");
  if (!lua_isnoneornil(state, -1)) {
    update.fullscreen = lua_toboolean(state, -1) != 0;
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "displayIndex");
  if (!lua_isnoneornil(state, -1)) {
    update.displayIndex = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "width");
  if (!lua_isnoneornil(state, -1)) {
    update.width = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "height");
  if (!lua_isnoneornil(state, -1)) {
    update.height = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "vsync");
  if (!lua_isnoneornil(state, -1)) {
    auto vsyncModeResult = StringToVsyncMode(luaL_checkstring(state, -1));
    if (Error::IsError(vsyncModeResult)) {
      return luaL_error(state, "%s", vsyncModeResult.error().message.c_str());
    }
    update.vsync = vsyncModeResult.value();
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "fullscreenMode");
  if (!lua_isnoneornil(state, -1)) {
    auto fullscreenModeResult = StringToFullscreenMode(luaL_checkstring(state, -1));
    if (Error::IsError(fullscreenModeResult)) {
      return luaL_error(state, "%s", fullscreenModeResult.error().message.c_str());
    }
    update.fullscreenMode = fullscreenModeResult.value();
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "minimumWidth");
  if (!lua_isnoneornil(state, -1)) {
    update.minimumWidth = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "minimumHeight");
  if (!lua_isnoneornil(state, -1)) {
    update.minimumHeight = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "xPosition");
  if (!lua_isnoneornil(state, -1)) {
    update.xPosition = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "yPosition");
  if (!lua_isnoneornil(state, -1)) {
    update.yPosition = static_cast<int>(luaL_checkinteger(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "colorspace");
  if (!lua_isnoneornil(state, -1)) {
    auto colorSpaceResult = StringToColorSpace(luaL_checkstring(state, -1));
    if (Error::IsError(colorSpaceResult)) {
      return luaL_error(state, "%s", colorSpaceResult.error().message.c_str());
    }
    update.colorSpace = colorSpaceResult.value();
  }
  lua_pop(state, 1);

  auto *wcontext = ::Window::GetWindowContext();

  ::Window::UpdateSettings(*wcontext, update);
  return 0;
}

// For the config file.
// Does not call SDL functions as the window is not yet created.
auto wrap_SetInitialSettings(lua_State *state) -> int {
  auto *wcontext = ::Window::GetWindowContext();

  auto settingsResult = SettingsFromStack(state);
  if (Error::IsError(settingsResult)) {
    return luaL_error(state, "%s", settingsResult.error().message.c_str());
  }

  wcontext->initialSettings = settingsResult.value();
  return 0;
}

auto wrap_GetTitle(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  std::string title = ::Window::GetTitle(window);
  lua_pushstring(state, title.c_str());
  return 1;
}

auto wrap_SetTitle(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  const char *title = luaL_checkstring(state, 1);
  ::Window::SetTitle(window, std::string(title));
  return 0;
}

auto wrap_GetPosition(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  Math::Ivec2 position = ::Window::GetPosition(window);
  lua_newtable(state);

  lua_pushinteger(state, static_cast<lua_Integer>(position.x));
  lua_setfield(state, -2, "x");

  lua_pushinteger(state, static_cast<lua_Integer>(position.y));
  lua_setfield(state, -2, "y");

  return 1;
}
auto wrap_SetPosition(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  int x_pos = static_cast<int>(luaL_checkinteger(state, 1));
  int y_pos = static_cast<int>(luaL_checkinteger(state, 2));
  ::Window::SetPosition(window, x_pos, y_pos);
  return 0;
}
auto wrap_SetVSync(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  auto vsyncModeResult = StringToVsyncMode(luaL_checkstring(state, 1));
  if (Error::IsError(vsyncModeResult)) {
    return luaL_error(state, "%s", vsyncModeResult.error().message.c_str());
  }

  auto vsync = vsyncModeResult.value();

  ::Window::SetVSync(window, vsync);
  return 0;
}
auto wrap_HasMouseFocus(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool hasFocus = ::Window::HasMouseFocus(window);
  lua_pushboolean(state, static_cast<int>(hasFocus));
  return 1;
}
auto wrap_HasKeyboardFocus(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool hasFocus = ::Window::HasKeyboardFocus(window);
  lua_pushboolean(state, static_cast<int>(hasFocus));
  return 1;
}
auto wrap_IsVisible(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool isVisible = ::Window::IsVisible(window);
  lua_pushboolean(state, static_cast<int>(isVisible));
  return 1;
}
auto wrap_IsOpen(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool isOpen = ::Window::IsOpen(window);
  lua_pushboolean(state, static_cast<int>(isOpen));
  return 1;
}
auto wrap_IsMinimized(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool isMinimized = ::Window::IsMinimized(window);
  lua_pushboolean(state, static_cast<int>(isMinimized));
  return 1;
}
auto wrap_IsMaximized(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool isMaximized = ::Window::IsMaximized(window);
  lua_pushboolean(state, static_cast<int>(isMaximized));
  return 1;
}
auto wrap_Minimize(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  ::Window::Minimize(window);
  return 0;
}
auto wrap_Maximize(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  ::Window::Maximize(window);
  return 0;
}
auto wrap_Restore(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  ::Window::Restore(window);
  return 0;
}
auto wrap_EnableDisplaySleep(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool enable = lua_toboolean(state, 1) != 0;
  ::Window::EnableDisplaySleep(window, enable);
  return 0;
}
auto wrap_IsDisplaySleepEnabled(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool isEnabled = ::Window::IsDisplaySleepEnabled(window);
  lua_pushboolean(state, static_cast<int>(isEnabled));
  return 1;
}
auto wrap_RequestAttention(lua_State *state) -> int {
  SDL_Window *window = ::Window::GetWindowContext()->window;

  bool continuous = lua_toboolean(state, 1) != 0;
  ::Window::RequestAttention(window, continuous);
  return 0;
}

auto wrap_SetColorSpace(lua_State *state) -> int {
  auto *wcontext = ::Window::GetWindowContext();

  auto colorSpaceResult = StringToColorSpace(luaL_checkstring(state, 1));
  if (Error::IsError(colorSpaceResult)) {
    return luaL_error(state, "%s", colorSpaceResult.error().message.c_str());
  }
  auto colorSpace = colorSpaceResult.value();

  ::Window::SetColorSpace(*wcontext, colorSpace);
  return 0;
}

auto wrap_GetColorSpace(lua_State *state) -> int {
  auto *wcontext = ::Window::GetWindowContext();

  ::Window::ColorSpace colorSpace = ::Window::GetColorSpace(*wcontext);
  const auto *colorSpaceStr = ColorSpaceToString(colorSpace);
  lua_pushstring(state, colorSpaceStr);
  return 1;
}

} // namespace Wrap::Window