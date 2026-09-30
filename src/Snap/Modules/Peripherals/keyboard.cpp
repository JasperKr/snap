#include "keyboard.hpp"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_keycode.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Keyboard {

constexpr int MaxKeycode = 512;
bool KeyRepeatEnabled = true; // NOLINT

inline auto Register(std::unordered_map<uint32_t, std::string> &vector,
                     uint32_t scancode, const std::string &name) -> void {
  vector[scancode] = name;
}

const std::unordered_map<uint32_t, std::string> KeycodeToStringMap =
    []() -> std::unordered_map<uint32_t, std::string> {
  std::unordered_map<uint32_t, std::string> vec(MaxKeycode + 1);
  Register(vec, SDLK_UNKNOWN, "unknown");

  Register(vec, SDLK_A, "a");
  Register(vec, SDLK_B, "b");
  Register(vec, SDLK_C, "c");
  Register(vec, SDLK_D, "d");
  Register(vec, SDLK_E, "e");
  Register(vec, SDLK_F, "f");
  Register(vec, SDLK_G, "g");
  Register(vec, SDLK_H, "h");
  Register(vec, SDLK_I, "i");
  Register(vec, SDLK_J, "j");
  Register(vec, SDLK_K, "k");
  Register(vec, SDLK_L, "l");
  Register(vec, SDLK_M, "m");
  Register(vec, SDLK_N, "n");
  Register(vec, SDLK_O, "o");
  Register(vec, SDLK_P, "p");
  Register(vec, SDLK_Q, "q");
  Register(vec, SDLK_R, "r");
  Register(vec, SDLK_S, "s");
  Register(vec, SDLK_T, "t");
  Register(vec, SDLK_U, "u");
  Register(vec, SDLK_V, "v");
  Register(vec, SDLK_W, "w");
  Register(vec, SDLK_X, "x");
  Register(vec, SDLK_Y, "y");
  Register(vec, SDLK_Z, "z");

  Register(vec, SDLK_1, "1");
  Register(vec, SDLK_2, "2");
  Register(vec, SDLK_3, "3");
  Register(vec, SDLK_4, "4");
  Register(vec, SDLK_5, "5");
  Register(vec, SDLK_6, "6");
  Register(vec, SDLK_7, "7");
  Register(vec, SDLK_8, "8");
  Register(vec, SDLK_9, "9");
  Register(vec, SDLK_0, "0");

  Register(vec, SDLK_RETURN, "return");
  Register(vec, SDLK_ESCAPE, "escape");
  Register(vec, SDLK_BACKSPACE, "backspace");
  Register(vec, SDLK_TAB, "tab");
  Register(vec, SDLK_SPACE, "space");

  // Punctuation and symbols
  Register(vec, SDLK_MINUS, "-");
  Register(vec, SDLK_EQUALS, "=");
  Register(vec, SDLK_LEFTBRACKET, "[");
  Register(vec, SDLK_RIGHTBRACKET, "]");
  Register(vec, SDLK_BACKSLASH, "\\");
  Register(vec, SDLK_HASH, "#");
  Register(vec, SDLK_SEMICOLON, ";");
  Register(vec, SDLK_APOSTROPHE, "'");
  Register(vec, SDLK_GRAVE, "`");
  Register(vec, SDLK_COMMA, ",");
  Register(vec, SDLK_PERIOD, ".");
  Register(vec, SDLK_SLASH, "/");
  // Lock keys
  Register(vec, SDLK_CAPSLOCK, "capslock");

  // Function keys
  Register(vec, SDLK_F1, "f1");
  Register(vec, SDLK_F2, "f2");
  Register(vec, SDLK_F3, "f3");
  Register(vec, SDLK_F4, "f4");
  Register(vec, SDLK_F5, "f5");
  Register(vec, SDLK_F6, "f6");
  Register(vec, SDLK_F7, "f7");
  Register(vec, SDLK_F8, "f8");
  Register(vec, SDLK_F9, "f9");
  Register(vec, SDLK_F10, "f10");
  Register(vec, SDLK_F11, "f11");
  Register(vec, SDLK_F12, "f12");

  // System keys
  Register(vec, SDLK_PRINTSCREEN, "printscreen");
  Register(vec, SDLK_SCROLLLOCK, "scrolllock");
  Register(vec, SDLK_PAUSE, "pause");
  Register(vec, SDLK_INSERT, "insert");
  Register(vec, SDLK_HOME, "home");
  Register(vec, SDLK_PAGEUP, "pageup");
  Register(vec, SDLK_DELETE, "delete");
  Register(vec, SDLK_END, "end");
  Register(vec, SDLK_PAGEDOWN, "pagedown");

  // Arrow keys
  Register(vec, SDLK_RIGHT, "right");
  Register(vec, SDLK_LEFT, "left");
  Register(vec, SDLK_DOWN, "down");
  Register(vec, SDLK_UP, "up");

  // Keypad
  Register(vec, SDLK_NUMLOCKCLEAR, "numlock");
  Register(vec, SDLK_KP_DIVIDE, "kp_/");
  Register(vec, SDLK_KP_MULTIPLY, "kp_*");
  Register(vec, SDLK_KP_MINUS, "kp_-");
  Register(vec, SDLK_KP_PLUS, "kp_+");
  Register(vec, SDLK_KP_ENTER, "kp_enter");
  Register(vec, SDLK_KP_1, "kp_1");
  Register(vec, SDLK_KP_2, "kp_2");
  Register(vec, SDLK_KP_3, "kp_3");
  Register(vec, SDLK_KP_4, "kp_4");
  Register(vec, SDLK_KP_5, "kp_5");
  Register(vec, SDLK_KP_6, "kp_6");
  Register(vec, SDLK_KP_7, "kp_7");
  Register(vec, SDLK_KP_8, "kp_8");
  Register(vec, SDLK_KP_9, "kp_9");
  Register(vec, SDLK_KP_0, "kp_0");
  Register(vec, SDLK_KP_PERIOD, "kp_.");

  // Additional keys
  Register(vec, SDLK_APPLICATION, "application");
  Register(vec, SDLK_POWER, "power");
  Register(vec, SDLK_KP_EQUALS, "kp_equals");

  // Extended function keys
  Register(vec, SDLK_F13, "f13");
  Register(vec, SDLK_F14, "f14");
  Register(vec, SDLK_F15, "f15");
  Register(vec, SDLK_F16, "f16");
  Register(vec, SDLK_F17, "f17");
  Register(vec, SDLK_F18, "f18");
  Register(vec, SDLK_F19, "f19");
  Register(vec, SDLK_F20, "f20");
  Register(vec, SDLK_F21, "f21");
  Register(vec, SDLK_F22, "f22");
  Register(vec, SDLK_F23, "f23");
  Register(vec, SDLK_F24, "f24");

  // System functions
  Register(vec, SDLK_EXECUTE, "execute");
  Register(vec, SDLK_HELP, "help");
  Register(vec, SDLK_MENU, "menu");
  Register(vec, SDLK_SELECT, "select");
  Register(vec, SDLK_STOP, "stop");
  Register(vec, SDLK_AGAIN, "again");
  Register(vec, SDLK_UNDO, "undo");
  Register(vec, SDLK_CUT, "cut");
  Register(vec, SDLK_COPY, "copy");
  Register(vec, SDLK_PASTE, "paste");
  Register(vec, SDLK_FIND, "find");

  // Audio controls
  Register(vec, SDLK_MUTE, "mute");
  Register(vec, SDLK_VOLUMEUP, "volumeup");
  Register(vec, SDLK_VOLUMEDOWN, "volumedown");

  // Additional keypad
  Register(vec, SDLK_KP_COMMA, "kp_,");
  Register(vec, SDLK_KP_EQUALSAS400, "kp_=");

  // System functions
  Register(vec, SDLK_ALTERASE, "alterase");
  Register(vec, SDLK_SYSREQ, "sysreq");
  Register(vec, SDLK_CANCEL, "cancel");
  Register(vec, SDLK_CLEAR, "clear");
  Register(vec, SDLK_PRIOR, "prior");
  Register(vec, SDLK_RETURN2, "return2");
  Register(vec, SDLK_SEPARATOR, "separator");
  Register(vec, SDLK_OUT, "out");
  Register(vec, SDLK_OPER, "oper");
  Register(vec, SDLK_CLEARAGAIN, "clearagain");
  Register(vec, SDLK_CRSEL, "crsel");
  Register(vec, SDLK_EXSEL, "exsel");

  // Extended keypad
  Register(vec, SDLK_KP_00, "kp_00");
  Register(vec, SDLK_KP_000, "kp_000");
  Register(vec, SDLK_THOUSANDSSEPARATOR, "thousandsseparator");
  Register(vec, SDLK_DECIMALSEPARATOR, "decimalseparator");
  Register(vec, SDLK_CURRENCYUNIT, "currencyunit");
  Register(vec, SDLK_CURRENCYSUBUNIT, "currencysubunit");
  Register(vec, SDLK_KP_LEFTPAREN, "kp_(");
  Register(vec, SDLK_KP_RIGHTPAREN, "kp_)");
  Register(vec, SDLK_KP_LEFTBRACE, "kp_{");
  Register(vec, SDLK_KP_RIGHTBRACE, "kp_}");
  Register(vec, SDLK_KP_TAB, "kp_tab");
  Register(vec, SDLK_KP_BACKSPACE, "kp_backspace");
  Register(vec, SDLK_KP_A, "kp_a");
  Register(vec, SDLK_KP_B, "kp_b");
  Register(vec, SDLK_KP_C, "kp_c");
  Register(vec, SDLK_KP_D, "kp_d");
  Register(vec, SDLK_KP_E, "kp_e");
  Register(vec, SDLK_KP_F, "kp_f");
  Register(vec, SDLK_KP_XOR, "kp_^");
  Register(vec, SDLK_KP_POWER, "kp_**");
  Register(vec, SDLK_KP_PERCENT, "kp_%");
  Register(vec, SDLK_KP_LESS, "kp_<");
  Register(vec, SDLK_KP_GREATER, "kp_>");
  Register(vec, SDLK_KP_AMPERSAND, "kp_&");
  Register(vec, SDLK_KP_DBLAMPERSAND, "kp_&&");
  Register(vec, SDLK_KP_VERTICALBAR, "kp_|");
  Register(vec, SDLK_KP_DBLVERTICALBAR, "kp_||");
  Register(vec, SDLK_KP_COLON, "kp_:");
  Register(vec, SDLK_KP_HASH, "kp_#");
  Register(vec, SDLK_KP_SPACE, "kp_ ");
  Register(vec, SDLK_KP_AT, "kp_@");
  Register(vec, SDLK_KP_EXCLAM, "kp_!");

  // Keypad memory functions
  Register(vec, SDLK_KP_MEMSTORE, "kp_memstore");
  Register(vec, SDLK_KP_MEMRECALL, "kp_memrecall");
  Register(vec, SDLK_KP_MEMCLEAR, "kp_memclear");
  Register(vec, SDLK_KP_MEMADD, "kp_memadd");
  Register(vec, SDLK_KP_MEMSUBTRACT, "kp_memsubtract");
  Register(vec, SDLK_KP_MEMMULTIPLY, "kp_memmultiply");
  Register(vec, SDLK_KP_MEMDIVIDE, "kp_memdivide");
  Register(vec, SDLK_KP_PLUSMINUS, "kp_+-");
  Register(vec, SDLK_KP_CLEAR, "kp_clear");
  Register(vec, SDLK_KP_CLEARENTRY, "kp_clearentry");

  // Keypad number systems
  Register(vec, SDLK_KP_BINARY, "kp_binary");
  Register(vec, SDLK_KP_OCTAL, "kp_octal");
  Register(vec, SDLK_KP_DECIMAL, "kp_decimal");
  Register(vec, SDLK_KP_HEXADECIMAL, "kp_hexadecimal");

  // Modifier keys
  Register(vec, SDLK_LCTRL, "lctrl");
  Register(vec, SDLK_LSHIFT, "lshift");
  Register(vec, SDLK_LALT, "lalt");
  Register(vec, SDLK_LGUI, "lgui");
  Register(vec, SDLK_RCTRL, "rctrl");
  Register(vec, SDLK_RSHIFT, "rshift");
  Register(vec, SDLK_RALT, "ralt");
  Register(vec, SDLK_RGUI, "rgui");

  // Mode key
  Register(vec, SDLK_MODE, "mode");

  // Media keys
  Register(vec, SDLK_MEDIA_NEXT_TRACK, "audionext");
  Register(vec, SDLK_MEDIA_PREVIOUS_TRACK, "audioprev");
  Register(vec, SDLK_MEDIA_STOP, "audiostop");
  Register(vec, SDLK_MEDIA_PLAY, "audioplay");
  Register(vec, SDLK_MUTE, "audiomute");
  Register(vec, SDLK_MEDIA_SELECT, "mediaselect");

  // Application control keys
  Register(vec, SDLK_AC_SEARCH, "ac_search");
  Register(vec, SDLK_AC_HOME, "ac_home");
  Register(vec, SDLK_AC_BACK, "ac_back");
  Register(vec, SDLK_AC_FORWARD, "ac_forward");
  Register(vec, SDLK_AC_STOP, "ac_stop");
  Register(vec, SDLK_AC_REFRESH, "ac_refresh");
  Register(vec, SDLK_AC_BOOKMARKS, "ac_bookmarks");

  // System keys
  Register(vec, SDLK_MEDIA_EJECT, "eject");
  Register(vec, SDLK_SLEEP, "sleep");

  return vec;
}();

const std::unordered_map<uint32_t, std::string> ScancodeToStringMap =
    []() -> std::unordered_map<uint32_t, std::string> {
  std::unordered_map<uint32_t, std::string> vec(MaxKeycode + 1);
  Register(vec, SDL_SCANCODE_UNKNOWN, "unknown");

  Register(vec, SDL_SCANCODE_A, "a");
  Register(vec, SDL_SCANCODE_B, "b");
  Register(vec, SDL_SCANCODE_C, "c");
  Register(vec, SDL_SCANCODE_D, "d");
  Register(vec, SDL_SCANCODE_E, "e");
  Register(vec, SDL_SCANCODE_F, "f");
  Register(vec, SDL_SCANCODE_G, "g");
  Register(vec, SDL_SCANCODE_H, "h");
  Register(vec, SDL_SCANCODE_I, "i");
  Register(vec, SDL_SCANCODE_J, "j");
  Register(vec, SDL_SCANCODE_K, "k");
  Register(vec, SDL_SCANCODE_L, "l");
  Register(vec, SDL_SCANCODE_M, "m");
  Register(vec, SDL_SCANCODE_N, "n");
  Register(vec, SDL_SCANCODE_O, "o");
  Register(vec, SDL_SCANCODE_P, "p");
  Register(vec, SDL_SCANCODE_Q, "q");
  Register(vec, SDL_SCANCODE_R, "r");
  Register(vec, SDL_SCANCODE_S, "s");
  Register(vec, SDL_SCANCODE_T, "t");
  Register(vec, SDL_SCANCODE_U, "u");
  Register(vec, SDL_SCANCODE_V, "v");
  Register(vec, SDL_SCANCODE_W, "w");
  Register(vec, SDL_SCANCODE_X, "x");
  Register(vec, SDL_SCANCODE_Y, "y");
  Register(vec, SDL_SCANCODE_Z, "z");

  Register(vec, SDL_SCANCODE_1, "1");
  Register(vec, SDL_SCANCODE_2, "2");
  Register(vec, SDL_SCANCODE_3, "3");
  Register(vec, SDL_SCANCODE_4, "4");
  Register(vec, SDL_SCANCODE_5, "5");
  Register(vec, SDL_SCANCODE_6, "6");
  Register(vec, SDL_SCANCODE_7, "7");
  Register(vec, SDL_SCANCODE_8, "8");
  Register(vec, SDL_SCANCODE_9, "9");
  Register(vec, SDL_SCANCODE_0, "0");

  Register(vec, SDL_SCANCODE_RETURN, "return");
  Register(vec, SDL_SCANCODE_ESCAPE, "escape");
  Register(vec, SDL_SCANCODE_BACKSPACE, "backspace");
  Register(vec, SDL_SCANCODE_TAB, "tab");
  Register(vec, SDL_SCANCODE_SPACE, "space");

  Register(vec, SDL_SCANCODE_MINUS, "-");
  Register(vec, SDL_SCANCODE_EQUALS, "=");
  Register(vec, SDL_SCANCODE_LEFTBRACKET, "[");
  Register(vec, SDL_SCANCODE_RIGHTBRACKET, "]");
  Register(vec, SDL_SCANCODE_BACKSLASH, "\\");
  Register(vec, SDL_SCANCODE_NONUSHASH, "#");
  Register(vec, SDL_SCANCODE_SEMICOLON, ";");
  Register(vec, SDL_SCANCODE_APOSTROPHE, "'");
  Register(vec, SDL_SCANCODE_GRAVE, "`");
  Register(vec, SDL_SCANCODE_COMMA, ",");
  Register(vec, SDL_SCANCODE_PERIOD, ".");
  Register(vec, SDL_SCANCODE_SLASH, "/");
  Register(vec, SDL_SCANCODE_CAPSLOCK, "capslock");

  Register(vec, SDL_SCANCODE_F1, "f1");
  Register(vec, SDL_SCANCODE_F2, "f2");
  Register(vec, SDL_SCANCODE_F3, "f3");
  Register(vec, SDL_SCANCODE_F4, "f4");
  Register(vec, SDL_SCANCODE_F5, "f5");
  Register(vec, SDL_SCANCODE_F6, "f6");
  Register(vec, SDL_SCANCODE_F7, "f7");
  Register(vec, SDL_SCANCODE_F8, "f8");
  Register(vec, SDL_SCANCODE_F9, "f9");
  Register(vec, SDL_SCANCODE_F10, "f10");
  Register(vec, SDL_SCANCODE_F11, "f11");
  Register(vec, SDL_SCANCODE_F12, "f12");

  Register(vec, SDL_SCANCODE_PRINTSCREEN, "printscreen");
  Register(vec, SDL_SCANCODE_SCROLLLOCK, "scrolllock");
  Register(vec, SDL_SCANCODE_PAUSE, "pause");
  Register(vec, SDL_SCANCODE_INSERT, "insert");
  Register(vec, SDL_SCANCODE_HOME, "home");
  Register(vec, SDL_SCANCODE_PAGEUP, "pageup");
  Register(vec, SDL_SCANCODE_DELETE, "delete");
  Register(vec, SDL_SCANCODE_END, "end");
  Register(vec, SDL_SCANCODE_PAGEDOWN, "pagedown");
  Register(vec, SDL_SCANCODE_RIGHT, "right");
  Register(vec, SDL_SCANCODE_LEFT, "left");
  Register(vec, SDL_SCANCODE_DOWN, "down");
  Register(vec, SDL_SCANCODE_UP, "up");

  Register(vec, SDL_SCANCODE_NUMLOCKCLEAR, "numlock");
  Register(vec, SDL_SCANCODE_KP_DIVIDE, "kp_/");
  Register(vec, SDL_SCANCODE_KP_MULTIPLY, "kp_*");
  Register(vec, SDL_SCANCODE_KP_MINUS, "kp_-");
  Register(vec, SDL_SCANCODE_KP_PLUS, "kp_+");
  Register(vec, SDL_SCANCODE_KP_ENTER, "kp_enter");
  Register(vec, SDL_SCANCODE_KP_1, "kp_1");
  Register(vec, SDL_SCANCODE_KP_2, "kp_2");
  Register(vec, SDL_SCANCODE_KP_3, "kp_3");
  Register(vec, SDL_SCANCODE_KP_4, "kp_4");
  Register(vec, SDL_SCANCODE_KP_5, "kp_5");
  Register(vec, SDL_SCANCODE_KP_6, "kp_6");
  Register(vec, SDL_SCANCODE_KP_7, "kp_7");
  Register(vec, SDL_SCANCODE_KP_8, "kp_8");
  Register(vec, SDL_SCANCODE_KP_9, "kp_9");
  Register(vec, SDL_SCANCODE_KP_0, "kp_0");
  Register(vec, SDL_SCANCODE_KP_PERIOD, "kp_.");

  Register(vec, SDL_SCANCODE_APPLICATION, "application");
  Register(vec, SDL_SCANCODE_POWER, "power");
  Register(vec, SDL_SCANCODE_KP_EQUALS, "kp_equals");
  Register(vec, SDL_SCANCODE_F13, "f13");
  Register(vec, SDL_SCANCODE_F14, "f14");
  Register(vec, SDL_SCANCODE_F15, "f15");
  Register(vec, SDL_SCANCODE_F16, "f16");
  Register(vec, SDL_SCANCODE_F17, "f17");
  Register(vec, SDL_SCANCODE_F18, "f18");
  Register(vec, SDL_SCANCODE_F19, "f19");
  Register(vec, SDL_SCANCODE_F20, "f20");
  Register(vec, SDL_SCANCODE_F21, "f21");
  Register(vec, SDL_SCANCODE_F22, "f22");
  Register(vec, SDL_SCANCODE_F23, "f23");
  Register(vec, SDL_SCANCODE_F24, "f24");

  Register(vec, SDL_SCANCODE_EXECUTE, "execute");
  Register(vec, SDL_SCANCODE_HELP, "help");
  Register(vec, SDL_SCANCODE_MENU, "menu");
  Register(vec, SDL_SCANCODE_SELECT, "select");
  Register(vec, SDL_SCANCODE_STOP, "stop");
  Register(vec, SDL_SCANCODE_AGAIN, "again");
  Register(vec, SDL_SCANCODE_UNDO, "undo");
  Register(vec, SDL_SCANCODE_CUT, "cut");
  Register(vec, SDL_SCANCODE_COPY, "copy");
  Register(vec, SDL_SCANCODE_PASTE, "paste");
  Register(vec, SDL_SCANCODE_FIND, "find");

  Register(vec, SDL_SCANCODE_MUTE, "mute");
  Register(vec, SDL_SCANCODE_VOLUMEUP, "volumeup");
  Register(vec, SDL_SCANCODE_VOLUMEDOWN, "volumedown");

  Register(vec, SDL_SCANCODE_KP_COMMA, "kp_,");
  Register(vec, SDL_SCANCODE_KP_EQUALSAS400, "kp_=");
  Register(vec, SDL_SCANCODE_ALTERASE, "alterase");
  Register(vec, SDL_SCANCODE_SYSREQ, "sysreq");
  Register(vec, SDL_SCANCODE_CANCEL, "cancel");
  Register(vec, SDL_SCANCODE_CLEAR, "clear");
  Register(vec, SDL_SCANCODE_PRIOR, "prior");
  Register(vec, SDL_SCANCODE_RETURN2, "return2");
  Register(vec, SDL_SCANCODE_SEPARATOR, "separator");
  Register(vec, SDL_SCANCODE_OUT, "out");
  Register(vec, SDL_SCANCODE_OPER, "oper");
  Register(vec, SDL_SCANCODE_CLEARAGAIN, "clearagain");
  Register(vec, SDL_SCANCODE_CRSEL, "crsel");
  Register(vec, SDL_SCANCODE_EXSEL, "exsel");

  Register(vec, SDL_SCANCODE_KP_00, "kp_00");
  Register(vec, SDL_SCANCODE_KP_000, "kp_000");
  Register(vec, SDL_SCANCODE_THOUSANDSSEPARATOR, "thousandsseparator");
  Register(vec, SDL_SCANCODE_DECIMALSEPARATOR, "decimalseparator");
  Register(vec, SDL_SCANCODE_CURRENCYUNIT, "currencyunit");
  Register(vec, SDL_SCANCODE_CURRENCYSUBUNIT, "currencysubunit");
  Register(vec, SDL_SCANCODE_KP_LEFTPAREN, "kp_(");
  Register(vec, SDL_SCANCODE_KP_RIGHTPAREN, "kp_)");
  Register(vec, SDL_SCANCODE_KP_LEFTBRACE, "kp_{");
  Register(vec, SDL_SCANCODE_KP_RIGHTBRACE, "kp_}");
  Register(vec, SDL_SCANCODE_KP_TAB, "kp_tab");
  Register(vec, SDL_SCANCODE_KP_BACKSPACE, "kp_backspace");

  Register(vec, SDL_SCANCODE_KP_A, "kp_a");
  Register(vec, SDL_SCANCODE_KP_B, "kp_b");
  Register(vec, SDL_SCANCODE_KP_C, "kp_c");
  Register(vec, SDL_SCANCODE_KP_D, "kp_d");
  Register(vec, SDL_SCANCODE_KP_E, "kp_e");
  Register(vec, SDL_SCANCODE_KP_F, "kp_f");
  Register(vec, SDL_SCANCODE_KP_XOR, "kp_^");
  Register(vec, SDL_SCANCODE_KP_POWER, "kp_**");
  Register(vec, SDL_SCANCODE_KP_PERCENT, "kp_%");
  Register(vec, SDL_SCANCODE_KP_LESS, "kp_<");
  Register(vec, SDL_SCANCODE_KP_GREATER, "kp_>");
  Register(vec, SDL_SCANCODE_KP_AMPERSAND, "kp_&");
  Register(vec, SDL_SCANCODE_KP_DBLAMPERSAND, "kp_&&");
  Register(vec, SDL_SCANCODE_KP_VERTICALBAR, "kp_|");
  Register(vec, SDL_SCANCODE_KP_DBLVERTICALBAR, "kp_||");
  Register(vec, SDL_SCANCODE_KP_COLON, "kp_:");
  Register(vec, SDL_SCANCODE_KP_HASH, "kp_#");
  Register(vec, SDL_SCANCODE_KP_SPACE, "kp_ ");
  Register(vec, SDL_SCANCODE_KP_AT, "kp_@");
  Register(vec, SDL_SCANCODE_KP_EXCLAM, "kp_!");

  Register(vec, SDL_SCANCODE_KP_MEMSTORE, "kp_memstore");
  Register(vec, SDL_SCANCODE_KP_MEMRECALL, "kp_memrecall");
  Register(vec, SDL_SCANCODE_KP_MEMCLEAR, "kp_memclear");
  Register(vec, SDL_SCANCODE_KP_MEMADD, "kp_memadd");
  Register(vec, SDL_SCANCODE_KP_MEMSUBTRACT, "kp_memsubtract");
  Register(vec, SDL_SCANCODE_KP_MEMMULTIPLY, "kp_memmultiply");
  Register(vec, SDL_SCANCODE_KP_MEMDIVIDE, "kp_memdivide");
  Register(vec, SDL_SCANCODE_KP_PLUSMINUS, "kp_+-");
  Register(vec, SDL_SCANCODE_KP_CLEAR, "kp_clear");
  Register(vec, SDL_SCANCODE_KP_CLEARENTRY, "kp_clearentry");

  Register(vec, SDL_SCANCODE_KP_BINARY, "kp_binary");
  Register(vec, SDL_SCANCODE_KP_OCTAL, "kp_octal");
  Register(vec, SDL_SCANCODE_KP_DECIMAL, "kp_decimal");
  Register(vec, SDL_SCANCODE_KP_HEXADECIMAL, "kp_hexadecimal");

  Register(vec, SDL_SCANCODE_LCTRL, "lctrl");
  Register(vec, SDL_SCANCODE_LSHIFT, "lshift");
  Register(vec, SDL_SCANCODE_LALT, "lalt");
  Register(vec, SDL_SCANCODE_LGUI, "lgui");
  Register(vec, SDL_SCANCODE_RCTRL, "rctrl");
  Register(vec, SDL_SCANCODE_RSHIFT, "rshift");
  Register(vec, SDL_SCANCODE_RALT, "ralt");
  Register(vec, SDL_SCANCODE_RGUI, "rgui");

  Register(vec, SDL_SCANCODE_MODE, "mode");
  Register(vec, SDL_SCANCODE_MEDIA_NEXT_TRACK, "audionext");
  Register(vec, SDL_SCANCODE_MEDIA_PREVIOUS_TRACK, "audioprev");
  Register(vec, SDL_SCANCODE_MEDIA_STOP, "audiostop");
  Register(vec, SDL_SCANCODE_MEDIA_PLAY, "audioplay");
  Register(vec, SDL_SCANCODE_MUTE, "audiomute");
  Register(vec, SDL_SCANCODE_MEDIA_SELECT, "mediaselect");
  Register(vec, SDL_SCANCODE_AC_SEARCH, "ac_search");
  Register(vec, SDL_SCANCODE_AC_HOME, "ac_home");
  Register(vec, SDL_SCANCODE_AC_BACK, "ac_back");
  Register(vec, SDL_SCANCODE_AC_FORWARD, "ac_forward");
  Register(vec, SDL_SCANCODE_AC_STOP, "ac_stop");
  Register(vec, SDL_SCANCODE_AC_REFRESH, "ac_refresh");
  Register(vec, SDL_SCANCODE_AC_BOOKMARKS, "ac_bookmarks");
  Register(vec, SDL_SCANCODE_MEDIA_EJECT, "eject");
  Register(vec, SDL_SCANCODE_SLEEP, "sleep");
  return vec;
}();

const std::unordered_map<std::string, SDL_Keycode> StringToKeycodeMap =
    []() -> std::unordered_map<std::string, SDL_Keycode> {
  std::unordered_map<std::string, SDL_Keycode> map;
  for (const auto &pair : KeycodeToStringMap) {
    map[pair.second] = static_cast<SDL_Keycode>(pair.first);
  }
  return map;
}();

const std::unordered_map<std::string, uint32_t> StringToScancodeMap =
    []() -> std::unordered_map<std::string, uint32_t> {
  std::unordered_map<std::string, uint32_t> map;
  for (const auto &pair : ScancodeToStringMap) {
    map[pair.second] = pair.first;
  }
  return map;
}();

auto IsDown(SDL_Keycode key) -> bool {
  const bool *state = SDL_GetKeyboardState(nullptr);
  SDL_Keymod mod = SDL_GetModState();

  // NOLINTNEXTLINE Pointer arithmetic on bool *
  return state[SDL_GetScancodeFromKey(key, &mod)];
}

auto IsDown(const std::string &keyName) -> bool {
  auto iterator = StringToKeycodeMap.find(keyName);
  if (iterator == StringToKeycodeMap.end()) {
    return false;
  }

  return IsDown(iterator->second);
}

auto IsScancodeDown(SDL_Scancode scancode) -> bool {
  const bool *state = SDL_GetKeyboardState(nullptr);
  // NOLINTNEXTLINE Pointer arithmetic on bool *
  return state[scancode];
}

auto IsScancodeDown(const std::string &scancodeName) -> bool {
  auto iterator = StringToScancodeMap.find(scancodeName);
  if (iterator == StringToScancodeMap.end()) {
    return false;
  }

  return IsScancodeDown(static_cast<SDL_Scancode>(iterator->second));
}

auto KeycodeToString(SDL_Keycode key) -> std::string {
  auto iterator = KeycodeToStringMap.find(static_cast<uint32_t>(key));
  if (iterator == KeycodeToStringMap.end()) {
    return "unknown";
  }
  return iterator->second;
}

auto ScancodeToString(SDL_Scancode scancode) -> std::string {
  auto iterator = ScancodeToStringMap.find(static_cast<uint32_t>(scancode));
  if (iterator == ScancodeToStringMap.end()) {
    return "unknown";
  }
  return iterator->second;
}

auto StringToKeycode(const std::string &keyName) -> SDL_Keycode {
  auto iterator = StringToKeycodeMap.find(keyName);
  if (iterator == StringToKeycodeMap.end()) {
    return SDLK_UNKNOWN;
  }
  return iterator->second;
}

auto StringToScancode(const std::string &scancodeName) -> SDL_Scancode {
  auto iterator = StringToScancodeMap.find(scancodeName);
  if (iterator == StringToScancodeMap.end()) {
    return SDL_SCANCODE_UNKNOWN;
  }
  return static_cast<SDL_Scancode>(iterator->second);
}

auto EnableKeyRepeat(bool repeat) -> void { KeyRepeatEnabled = repeat; }

} // namespace Keyboard