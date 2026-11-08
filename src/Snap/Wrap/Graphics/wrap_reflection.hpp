#pragma once

#include "Graphics/graphicsState.hpp"
#include "Modules/error.hpp"
#include "lua.hpp"
#include <lauxlib.h>
#include <lua.h>

inline auto ResourceKeyFromLuaTable(lua_State *state, int index) -> Result<Graphics::ResourceKey> {
  if (index < 0) {
    index = lua_gettop(state) + index + 1;
  }

  Graphics::ResourceKey root;

  luaL_checktype(state, index, LUA_TTABLE);
  size_t tableSize = lua_objlen(state, index);

  for (size_t i = 0; i < tableSize; ++i) {
    lua_rawgeti(state, index, static_cast<int>(i + 1));

    if (lua_type(state, -1) == LUA_TSTRING) {
      const char *keyPart = luaL_checkstring(state, -1);
      root.emplace_back(luaL_checkstring(state, -1));
    } else if (lua_type(state, -1) == LUA_TNUMBER) {
      auto indexValue = luaL_checkinteger(state, -1);
      if (root.empty()) {
        lua_pop(state, 1);
        return Error::Create("Invalid resource key: indexing without a preceding key part");
      }

      root.back().Index = static_cast<uint64_t>(indexValue);
    } else {
      lua_pop(state, 1);
      return Error::Create("Invalid resource key: expected string or number in table");
    }

    lua_pop(state, 1);
  }

  return root;
}

inline auto ResourceKeyFromSingleLuaObject(lua_State *state, int index) -> Result<Graphics::ResourceKey> {
  // Just a single string, treat it as a key with one part
  if (lua_type(state, index) == LUA_TSTRING) {
    return Graphics::ResourceKey{luaL_checkstring(state, index)};
  }
  // A table, treat it as a list of strings
  if (lua_type(state, index) == LUA_TTABLE) {
    return ResourceKeyFromLuaTable(state, index);
  }
  // Invalid type, return empty key
  return Graphics::ResourceKey{};
}