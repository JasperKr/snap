#pragma once

#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vector>

namespace LuaWrap {

template <typename T> auto PushVector(lua_State *state, const std::vector<T> &value) -> void {
  lua_newtable(state);
  for (std::size_t i = 0; i < value.size(); ++i) {
    PushObject(state, T::GetType(), value[i]);
    lua_rawseti(state, -2, static_cast<int>(i + 1));
  }
}

template <typename T> inline auto VectorFromLua(lua_State *state, int index) -> Result<std::vector<T>> {
  if (!lua_istable(state, index)) {
    return Error::Createf("Expected a table at index {}", index);
  }

  std::vector<T> result;
  size_t length = lua_objlen(state, index);
  for (size_t i = 1; i <= length; ++i) {
    lua_rawgeti(state, index, static_cast<int>(i));
    auto elementResult = ObjectFromLua<T>(state, -1);
    if (!elementResult) {
      return Error::Createf("Failed to convert element at index {}: {}", i, elementResult.Error().Message());
    }
    result.emplace_back(elementResult.Value());
    lua_pop(state, 1); // Pop the element
  }

  return result;
}

template <typename T, typename ToLuaFn> auto PushVector(lua_State *state, const std::vector<T> &value, ToLuaFn toLua) -> void {
  lua_newtable(state);
  for (std::size_t i = 0; i < value.size(); ++i) {
    toLua(state, value[i]);
    lua_rawseti(state, -2, static_cast<int>(i + 1));
  }
}

template <typename T, typename FromLuaFn> inline auto VectorFromLua(lua_State *state, int index, FromLuaFn fromLua) -> Result<std::vector<T>> {
  if (!lua_istable(state, index)) {
    return Error::Createf("Expected a table at index {}", index);
  }
  std::vector<T> result;
  size_t length = lua_objlen(state, index);
  for (size_t i = 1; i <= length; ++i) {
    lua_rawgeti(state, index, static_cast<int>(i));
    auto elementResult = fromLua(state, -1);
    if (!elementResult) {
      return Error::Createf("Failed to convert element at index {}: {}", i, elementResult.Error().Message());
    }
    result.emplace_back(elementResult.Value());
    lua_pop(state, 1);
  }
  return result;
}

} // namespace LuaWrap