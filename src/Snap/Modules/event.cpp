#include "event.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/window.hpp"
#include "SDL3/SDL_events.h"
#include <format>
#include <mutex>
#include <queue>

namespace Event {

// NOLINTNEXTLINE
bool MainLoopRunning = true;

// NOLINTNEXTLINE
std::queue<Event> Events;

// Events can be pushed from multiple threads (only in the case of an error event, so this mutex won't be contended basically ever)
// But just to be safe if is here.
// NOLINTNEXTLINE
std::mutex EventsMutex;

inline auto FromSDLEvent(const SDL_Event &sdlEvent) -> Event {
  Event event;

  switch (sdlEvent.type) {
  case SDL_EVENT_KEY_DOWN: {
    event.Name = "keypressed";
    auto keyCode = Keyboard::KeycodeToString(sdlEvent.key.key);
    auto scanCode = Keyboard::ScancodeToString(sdlEvent.key.scancode);
    if (sdlEvent.key.repeat && !Keyboard::KeyRepeatEnabled) {
      return event;
    }
    event.Values.emplace_back(keyCode);
    event.Values.emplace_back(scanCode);
    event.Values.emplace_back(static_cast<bool>(sdlEvent.key.repeat));
    break;
  }
  case SDL_EVENT_KEY_UP: {
    event.Name = "keyreleased";
    auto keyCode = Keyboard::KeycodeToString(sdlEvent.key.key);
    auto scanCode = Keyboard::ScancodeToString(sdlEvent.key.scancode);
    event.Values.emplace_back(keyCode);
    event.Values.emplace_back(scanCode);
    break;
  }
  case SDL_EVENT_FINGER_DOWN:
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    event.Name = "mousepressed";
    event.Values.emplace_back(sdlEvent.button.x);
    event.Values.emplace_back(sdlEvent.button.y);
    event.Values.emplace_back(static_cast<uint32_t>(sdlEvent.button.button));
    event.Values.emplace_back(sdlEvent.type == SDL_EVENT_FINGER_DOWN);
    event.Values.emplace_back(static_cast<uint32_t>(sdlEvent.button.clicks));
    break;
  }
  case SDL_EVENT_FINGER_UP:
  case SDL_EVENT_MOUSE_BUTTON_UP: {
    event.Name = "mousereleased";
    event.Values.emplace_back(sdlEvent.button.x);
    event.Values.emplace_back(sdlEvent.button.y);
    event.Values.emplace_back(static_cast<uint32_t>(sdlEvent.button.button));
    event.Values.emplace_back(sdlEvent.type == SDL_EVENT_FINGER_UP);
    break;
  }
  case SDL_EVENT_MOUSE_MOTION: {
    event.Name = "mousemoved";
    event.Values.emplace_back(sdlEvent.motion.x);
    event.Values.emplace_back(sdlEvent.motion.y);
    event.Values.emplace_back(sdlEvent.motion.xrel);
    event.Values.emplace_back(sdlEvent.motion.yrel);
    break;
  }
  case SDL_EVENT_MOUSE_WHEEL: {
    event.Name = "wheelmoved";
    event.Values.emplace_back(sdlEvent.wheel.x);
    event.Values.emplace_back(sdlEvent.wheel.y);
    break;
  }
  case SDL_EVENT_TEXT_INPUT: {
    event.Name = "textinput";
    event.Values.emplace_back(std::string(sdlEvent.text.text));
    break;
  }
  case SDL_EVENT_TERMINATING:
  case SDL_EVENT_QUIT: {
    event.Name = "quit";
    break;
  }
  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
  case SDL_EVENT_WINDOW_RESIZED: {
    event.Name = "resize";
    event.Values.emplace_back(sdlEvent.window.data1);
    event.Values.emplace_back(sdlEvent.window.data2);
    Window::GetWindowContext()->swapchainOutOfDate = true;
    break;
  }
  case SDL_EVENT_LOW_MEMORY: {
    event.Name = "lowmemory";
    break;
  }
  case SDL_EVENT_LOCALE_CHANGED: {
    event.Name = "localechanged";
    break;
  }
  case SDL_EVENT_SYSTEM_THEME_CHANGED: {
    event.Name = "themechanged";
    break;
  }
  default:
    event.Name = std::format("SDL_EventType{}", std::to_string(sdlEvent.type));
    break;
  }

  return event;
}

auto Push(const Event &event) -> void {
  std::lock_guard<std::mutex> lock(EventsMutex);
  Events.emplace(event);
}

auto Pull() -> void {
  std::lock_guard<std::mutex> lock(EventsMutex);
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    Events.emplace(FromSDLEvent(event));
  }
}

auto Pop() -> std::optional<Event> {
  std::lock_guard<std::mutex> lock(EventsMutex);
  if (Events.empty()) {
    return std::nullopt;
  }

  Event event = Events.front();
  Events.pop();

  return event;
}

auto Quit() -> void { MainLoopRunning = false; }

} // namespace Event
