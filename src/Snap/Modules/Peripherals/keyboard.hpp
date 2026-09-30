#include "SDL3/SDL_keycode.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Keyboard {

extern const std::unordered_map<uint32_t, std::string> KeycodeToStringMap;
extern const std::unordered_map<uint32_t, std::string> ScancodeToStringMap;
extern const std::unordered_map<std::string, SDL_Keycode> StringToKeycodeMap;
extern const std::unordered_map<std::string, uint32_t> StringToScancodeMap;
extern bool KeyRepeatEnabled; // NOLINT

auto IsDown(SDL_Keycode key) -> bool;
auto IsDown(const std::string &keyName) -> bool;
auto IsScancodeDown(SDL_Scancode scancode) -> bool;
auto IsScancodeDown(const std::string &scancodeName) -> bool;
auto KeycodeToString(SDL_Keycode key) -> std::string;
auto ScancodeToString(SDL_Scancode scancode) -> std::string;
auto StringToKeycode(const std::string &keyName) -> SDL_Keycode;
auto StringToScancode(const std::string &scancodeName) -> SDL_Scancode;
auto EnableKeyRepeat(bool repeat) -> void;

} // namespace Keyboard