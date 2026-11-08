#pragma once

#include "Modules/Math/vector.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "SDL3/SDL_video.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace Window {

// Hide the window, keeping the application running in the background.
auto Hide(SDL_Window *window) -> void;

auto GetDisplayCount(SDL_Window *window) -> int;
auto GetDisplayName(SDL_Window *window, int displayIndex) -> std::string;
auto IsFullscreen(SDL_Window *window) -> bool;
auto GetFullscreenDimensions(SDL_Window *window, int displayIndex) -> std::vector<Math::Uvec2>;
auto SetFullscreen(SDL_Window *window, bool fullscreen) -> void;
auto GetWidth(SDL_Window *window) -> int;
auto GetHeight(SDL_Window *window) -> int;
auto GetDimensions(SDL_Window *window) -> Math::Uvec2;
auto SetDimensions(SDL_Window *window, int width, int height) -> void;
auto SetIcon(SDL_Window *window, const Ref<Image::ImageData> &icon) -> void;

enum class FullscreenMode : uint8_t {
  Windowed = 0,
  Fullscreen = 1,
  Borderless = 2,
};

enum class VsyncMode : uint8_t {
  // Immediately present frames, no vsync, screen tearing may occur
  // Lowest latency option
  Immediate, // AKA: VK_PRESENT_MODE_IMMEDIATE_KHR
  // Wait on vertical blanking, no framerate limit, no tearing
  // Standard non-vsync behavior
  Replace, // AKA: VK_PRESENT_MODE_MAILBOX_KHR
  // Wait on vertical blanking, limit framerate to display refresh rate
  // Standard vsync behavior
  Enabled, // AKA: VK_PRESENT_MODE_FIFO_KHR
  // Dynamically enable/disable vsync based on performance, may introduce slight tearing
  // Useful for reducing stutter
  Adaptive, // AKA: VK_PRESENT_MODE_FIFO_RELAXED_KHR
};

enum class ColorSpace : uint8_t { GammaCorrect, Linear, HDR };

struct Settings {
  std::string title{"snap Application"};
  bool resizable{true};
  bool borderless{false};
  bool fullscreen{false};
  int displayIndex{1};
  int width{800};  // NOLINT
  int height{600}; // NOLINT
  VsyncMode vsync{VsyncMode::Enabled};
  FullscreenMode fullscreenMode{FullscreenMode::Windowed};
  int minimumWidth{1};
  int minimumHeight{1};
  int xPosition{SDL_WINDOWPOS_CENTERED}; // NOLINT
  int yPosition{SDL_WINDOWPOS_CENTERED}; // NOLINT

  ColorSpace colorSpace{ColorSpace::GammaCorrect};

  [[nodiscard]] auto GetSDLWindowFlags() const -> Uint32 {
    Uint32 flags = SDL_WINDOW_VULKAN;
    if (resizable) {
      flags |= SDL_WINDOW_RESIZABLE;
    }
    if (borderless) {
      flags |= SDL_WINDOW_BORDERLESS;
    }
    if (fullscreen) {
      flags |= SDL_WINDOW_FULLSCREEN;
    }
    return flags;
  }
};

struct SettingsUpdate {
  std::optional<std::string> title;
  std::optional<bool> resizable;
  std::optional<bool> borderless;
  std::optional<bool> fullscreen;
  std::optional<int> displayIndex;
  std::optional<int> width;
  std::optional<int> height;

  std::optional<VsyncMode> vsync;
  std::optional<FullscreenMode> fullscreenMode;
  std::optional<int> minimumWidth;
  std::optional<int> minimumHeight;
  std::optional<int> xPosition;
  std::optional<int> yPosition;

  std::optional<ColorSpace> colorSpace;
};

struct WindowContext {
  SDL_Window *window{nullptr};
  VsyncMode vsync{VsyncMode::Enabled};
  ColorSpace colorSpace{ColorSpace::GammaCorrect};

  bool swapchainOutOfDate{false};
  Settings initialSettings{};
};

auto GetSettings(WindowContext &wcontext) -> Settings;
auto SetSettings(WindowContext &wcontext, const Settings &settings) -> void;
auto UpdateSettings(WindowContext &wcontext, const SettingsUpdate &update) -> void;

auto SetColorSpace(WindowContext &window, ColorSpace colorSpace) -> void;
auto GetColorSpace(WindowContext &window) -> ColorSpace;

auto GetTitle(SDL_Window *window) -> std::string;
auto SetTitle(SDL_Window *window, const std::string &title) -> void;

auto GetPosition(SDL_Window *window) -> Math::Ivec2;
auto SetPosition(SDL_Window *window, int x_pos, int y_pos) -> void;

auto GetVSync(SDL_Window *window) -> VsyncMode;
auto SetVSync(SDL_Window *window, VsyncMode vsync) -> void;

auto HasMouseFocus(SDL_Window *window) -> bool;
auto HasKeyboardFocus(SDL_Window *window) -> bool;
auto IsVisible(SDL_Window *window) -> bool;

auto IsOpen(SDL_Window *window) -> bool;
auto IsMinimized(SDL_Window *window) -> bool;
auto IsMaximized(SDL_Window *window) -> bool;
auto Minimize(SDL_Window *window) -> void;
auto Maximize(SDL_Window *window) -> void;
auto Restore(SDL_Window *window) -> void;

auto EnableDisplaySleep(SDL_Window *window, bool enable) -> void;
auto IsDisplaySleepEnabled(SDL_Window *window) -> bool;

auto RequestAttention(SDL_Window *window, bool continuous) -> void;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern WindowContext *global_window_context;

auto GetWindowContext() -> WindowContext *;
auto SetWindowContext(WindowContext &context) -> void;

} // namespace Window