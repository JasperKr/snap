#pragma once

#include "Modules/color.hpp"
#include "lua.hpp"
#include <cstdint>

enum ColorFormat : uint8_t {
  List,
  VarArg,
};

template <typename T>
static inline auto ReadNumberFromTable(lua_State *state,
                                       int tableIndex, // NOLINT
                                       int keyIndex) -> T {
  lua_pushinteger(state, keyIndex);
  lua_gettable(state, tableIndex);
  auto value = static_cast<T>(luaL_checknumber(state, -1));
  lua_pop(state, 1);
  return value;
}

static inline auto ColorFromLuaState(lua_State *state, ColorFormat format, int startIndex) -> Color {

  if (startIndex < 0) {
    startIndex = lua_gettop(state) + startIndex + 1;
  }

  switch (format) {
  case ColorFormat::List: {
    // Expecting a table at startIndex
    luaL_checktype(state, startIndex, LUA_TTABLE);
    auto red = ReadNumberFromTable<float>(state, startIndex, 1);
    auto green = ReadNumberFromTable<float>(state, startIndex, 2);
    auto blue = ReadNumberFromTable<float>(state, startIndex, 3);
    auto alpha = ReadNumberFromTable<float>(state, startIndex, 4);

    return {red, green, blue, alpha};
  }
  case ColorFormat::VarArg: {
    auto red = static_cast<float>(luaL_checknumber(state, startIndex));
    auto green = static_cast<float>(luaL_checknumber(state, startIndex + 1));
    auto blue = static_cast<float>(luaL_checknumber(state, startIndex + 2));
    auto alpha = static_cast<float>(luaL_checknumber(state, startIndex + 3));

    return {red, green, blue, alpha};
  }
  }

  return Color{}; // Impossible to reach but windows compiler is stupid af
}