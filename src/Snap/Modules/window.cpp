#include "window.hpp"

#include "Graphics/graphicsState.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include <cassert>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Window {
// Hide the window, keeping the application running in the background.
auto Hide(SDL_Window *window) -> void { SDL_HideWindow(window); }

auto GetDisplayCount(SDL_Window *window) -> int {
  int displayCount = 0;
  SDL_GetDisplays(&displayCount);
  return displayCount;
}
auto GetDisplayName(SDL_Window *window, int displayIndex) -> std::string {
  const char *name = SDL_GetDisplayName(displayIndex);
  return (name != nullptr) ? std::string(name) : std::string();
}
auto IsFullscreen(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_FULLSCREEN) != 0;
}
auto GetFullscreenDimensions(SDL_Window *window, int displayIndex) -> std::vector<Math::Uvec2> {
  std::vector<Math::Uvec2> resolutions;
  int modeCount = 0;
  auto **modes = SDL_GetFullscreenDisplayModes(displayIndex, &modeCount);
  for (int i = 0; i < modeCount; ++i) {
    SDL_DisplayMode mode = *modes[i]; // NOLINT pointer arithmetic
    resolutions.emplace_back(static_cast<uint32_t>(mode.w), static_cast<uint32_t>(mode.h));
  }

  return resolutions;
}

auto SetFullscreen(SDL_Window *window, bool fullscreen) -> void {
  if (fullscreen) {
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
  } else {
    SDL_SetWindowFullscreen(window, false);
  }
}

auto GetWidth(SDL_Window *window) -> int {
  int width = 0;
  SDL_GetWindowSize(window, &width, nullptr);
  return width;
}

auto GetHeight(SDL_Window *window) -> int {
  int height = 0;
  SDL_GetWindowSize(window, nullptr, &height);
  return height;
}

auto GetDimensions(SDL_Window *window) -> Math::Uvec2 {
  int width = 0;
  int height = 0;
  SDL_GetWindowSize(window, &width, &height);
  return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

auto SetDimensions(SDL_Window *window, int width, int height) -> void { SDL_SetWindowSize(window, width, height); }

auto SetIcon(SDL_Window *window, const Ref<Image::ImageData> &icon) -> void {
  if (icon.get() != nullptr && icon->GetFormat() == VK_FORMAT_R8G8B8A8_UNORM) {
    SDL_Surface *surface = SDL_CreateSurfaceFrom(static_cast<int>(icon->GetWidth()), static_cast<int>(icon->GetHeight()), SDL_PIXELFORMAT_RGBA8888,
                                                 icon->GetDataPtr(), static_cast<int>(icon->GetWidth() * 4));

    if (surface != nullptr) {
      SDL_SetWindowIcon(window, surface);
      SDL_DestroySurface(surface);
    }
  }
}

auto GetSettings(WindowContext &wcontext) -> Settings {
  SDL_Window *window = wcontext.window;

  Settings settings;
  Uint32 flags = SDL_GetWindowFlags(window);
  settings.resizable = (flags & SDL_WINDOW_RESIZABLE) != 0;
  settings.borderless = (flags & SDL_WINDOW_BORDERLESS) != 0;
  settings.fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
  settings.displayIndex = static_cast<int>(SDL_GetDisplayForWindow(window));
  if (settings.displayIndex == 0) {
    settings.displayIndex = 1;
  }
  settings.width = GetWidth(window);
  settings.height = GetHeight(window);
  settings.vsync = wcontext.vsync;

  settings.fullscreenMode = FullscreenMode::Windowed;

  if (settings.fullscreen) {
    const auto *mode = SDL_GetWindowFullscreenMode(window);
    settings.fullscreenMode = (mode == nullptr) ? FullscreenMode::Borderless : FullscreenMode::Fullscreen;
  }
  bool success = SDL_GetWindowMinimumSize(window, &settings.minimumWidth, &settings.minimumHeight);
  if (!success) {
    settings.minimumWidth = 0;
    settings.minimumHeight = 0;
  }

  SDL_GetWindowPosition(window, &settings.xPosition, &settings.yPosition);

  return settings;
}

auto SetSettings(WindowContext &wcontext, const Settings &settings) -> void {
  SDL_Window *window = wcontext.window;

  SDL_SetWindowResizable(window, settings.resizable);
  SDL_SetWindowBordered(window, !settings.borderless);
  SetFullscreen(window, settings.fullscreen);
  SetDimensions(window, settings.width, settings.height);

  wcontext.vsync = settings.vsync;
  wcontext.swapchainOutOfDate = true;

  SDL_SetWindowMinimumSize(window, settings.minimumWidth, settings.minimumHeight);
  SDL_SetWindowPosition(window, settings.xPosition, settings.yPosition);

  SetColorSpace(wcontext, settings.colorSpace);
}

auto UpdateSettings(WindowContext &wcontext, const SettingsUpdate &update) -> void {
  Settings currentSettings = GetSettings(wcontext);
  Settings newSettings = currentSettings;

  if (update.resizable.has_value()) {
    newSettings.resizable = update.resizable.value();
  }
  if (update.borderless.has_value()) {
    newSettings.borderless = update.borderless.value();
  }
  if (update.fullscreen.has_value()) {
    newSettings.fullscreen = update.fullscreen.value();
  }
  if (update.displayIndex.has_value()) {
    newSettings.displayIndex = update.displayIndex.value();
  }
  if (update.width.has_value()) {
    newSettings.width = update.width.value();
  }
  if (update.height.has_value()) {
    newSettings.height = update.height.value();
  }
  if (update.vsync.has_value()) {
    newSettings.vsync = update.vsync.value();
  }
  if (update.fullscreenMode.has_value()) {
    newSettings.fullscreenMode = update.fullscreenMode.value();
  }
  if (update.minimumWidth.has_value()) {
    newSettings.minimumWidth = update.minimumWidth.value();
  }
  if (update.minimumHeight.has_value()) {
    newSettings.minimumHeight = update.minimumHeight.value();
  }
  if (update.xPosition.has_value()) {
    newSettings.xPosition = update.xPosition.value();
  }
  if (update.yPosition.has_value()) {
    newSettings.yPosition = update.yPosition.value();
  }

  SetSettings(wcontext, newSettings);
}

auto GetTitle(SDL_Window *window) -> std::string {
  const char *title = SDL_GetWindowTitle(window);
  return (title != nullptr) ? std::string(title) : std::string();
}
auto SetTitle(SDL_Window *window, const std::string &title) -> void { SDL_SetWindowTitle(window, title.c_str()); }

auto GetPosition(SDL_Window *window) -> Math::Ivec2 {
  Math::Ivec2 pos{};
  SDL_GetWindowPosition(window, &pos.x, &pos.y);
  return pos;
}
auto SetPosition(SDL_Window *window, int x_pos, int y_pos) -> void { SDL_SetWindowPosition(window, x_pos, y_pos); }

auto SetVSync(SDL_Window *window, VsyncMode vsync) -> void {
  WindowContext *wcontext = GetWindowContext();
  wcontext->vsync = vsync;
  wcontext->swapchainOutOfDate = true;
}

auto HasMouseFocus(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_MOUSE_FOCUS) != 0;
}

auto HasKeyboardFocus(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_INPUT_FOCUS) != 0;
}

auto IsVisible(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_OCCLUDED) != 0;
}

auto IsOpen(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_HIDDEN) != 0;
}

auto IsMinimized(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_MINIMIZED) != 0;
}
auto IsMaximized(SDL_Window *window) -> bool {
  Uint32 flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}
auto Minimize(SDL_Window *window) -> void { SDL_MinimizeWindow(window); }
auto Maximize(SDL_Window *window) -> void { SDL_MaximizeWindow(window); }
auto Restore(SDL_Window *window) -> void { SDL_RestoreWindow(window); }

auto EnableDisplaySleep(SDL_Window *window, bool enable) -> void {
  if (enable) {
    SDL_EnableScreenSaver();
  } else {
    SDL_DisableScreenSaver();
  }
}

auto IsDisplaySleepEnabled(SDL_Window *window) -> bool { return SDL_ScreenSaverEnabled(); }

auto RequestAttention(SDL_Window *window, bool continuous) -> void { SDL_FlashWindow(window, continuous ? SDL_FLASH_UNTIL_FOCUSED : SDL_FLASH_BRIEFLY); }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
WindowContext *global_window_context;

auto GetWindowContext() -> WindowContext * { return global_window_context; }
auto SetWindowContext(WindowContext &context) -> void { global_window_context = &context; }

auto SetColorSpace(WindowContext &window, ColorSpace colorSpace) -> void {
  window.colorSpace = colorSpace;
  window.swapchainOutOfDate = true;

  Graphics::DefaultPixelFormat = (colorSpace == ColorSpace::Linear) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
}
auto GetColorSpace(WindowContext &window) -> ColorSpace { return window.colorSpace; }

} // namespace Window