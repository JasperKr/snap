#pragma once

#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <unordered_map>

namespace LuaWrap {

template <typename K> inline auto ConvertLuaKey(lua_State *state, int index) -> Result<K> {
  if constexpr (std::is_same_v<K, std::string> || std::is_same_v<K, const char *>) {
    if (!lua_isstring(state, index)) {
      return Error::Createf("Expected string keys in the table at index {}", index);
    }
    return lua_tostring(state, index);
  } else if constexpr (std::is_same_v<K, double>) {
    if (!lua_isnumber(state, index)) {
      return Error::Createf("Expected number keys in the table at index {}", index);
    }
    return lua_tonumber(state, index);
  } else if constexpr (std::is_same_v<K, int>) {
    if (!lua_isnumber(state, index)) {
      return Error::Createf("Expected integer keys in the table at index {}", index);
    }
    return lua_tointeger(state, index);
  } else if constexpr (std::is_same_v<K, bool>) {
    if (!lua_isboolean(state, index)) {
      return Error::Createf("Expected boolean keys in the table at index {}", index);
    }
    return lua_toboolean(state, index) != 0;
  } else {
    auto keyResult = ObjectFromLua<K>(state, index);
    if (!keyResult) {
      return Error::Createf("Failed to convert key at index {}: {}", index, keyResult.Error().Message());
    }
    return keyResult.Value();
  }
}

template <typename V> inline auto ConvertLuaValue(lua_State *state, int index) -> Result<V> {
  if constexpr (std::is_same_v<V, std::string> || std::is_same_v<V, const char *>) {
    if (!lua_isstring(state, index)) {
      return Error::Createf("Expected string values in the table at index {}", index);
    }
    return lua_tostring(state, index);
  } else if constexpr (std::is_same_v<V, double>) {
    if (!lua_isnumber(state, index)) {
      return Error::Createf("Expected number values in the table at index {}", index);
    }
    return lua_tonumber(state, index);
  } else if constexpr (std::is_same_v<V, int>) {
    if (!lua_isnumber(state, index)) {
      return Error::Createf("Expected integer values in the table at index {}", index);
    }
    return lua_tointeger(state, index);
  } else if constexpr (std::is_same_v<V, bool>) {
    if (!lua_isboolean(state, index)) {
      return Error::Createf("Expected boolean values in the table at index {}", index);
    }
    return lua_toboolean(state, index) != 0;
  } else {
    auto valueResult = ObjectFromLua<V>(state, index);
    if (!valueResult) {
      return Error::Createf("Failed to convert value at index {}: {}", index, valueResult.Error().Message());
    }
    return valueResult.Value();
  }
}

template <typename K, typename V, typename H = std::hash<K>> auto PushMap(lua_State *state, const std::unordered_map<K, V, H> &value) -> void {
  lua_newtable(state);
  for (std::size_t i = 0; i < value.size(); ++i) {
    // Push the key
    if constexpr (std::is_same_v<K, std::string>) {
      lua_pushstring(state, value[i].first.c_str());
    } else if constexpr (std::is_same_v<K, const char *>) {
      lua_pushstring(state, value[i].first);
    } else if constexpr (std::is_same_v<K, double>) {
      lua_pushnumber(state, value[i].first);
    } else if constexpr (std::is_same_v<K, int>) {
      lua_pushinteger(state, value[i].first);
    } else if constexpr (std::is_same_v<K, bool>) {
      lua_pushboolean(state, value[i].first);
    } else {
      PushObject(state, K::GetType(), value[i].first);
    }

    // Push the value
    if constexpr (std::is_same_v<V, std::string>) {
      lua_pushstring(state, value[i].second.c_str());
    } else if constexpr (std::is_same_v<V, const char *>) {
      lua_pushstring(state, value[i].second);
    } else if constexpr (std::is_same_v<V, double>) {
      lua_pushnumber(state, value[i].second);
    } else if constexpr (std::is_same_v<V, int>) {
      lua_pushinteger(state, value[i].second);
    } else if constexpr (std::is_same_v<V, bool>) {
      lua_pushboolean(state, value[i].second);
    } else {
      PushObject(state, V::GetType(), value[i].second);
    }

    lua_rawset(state, -3);
  }
}
template <typename K, typename V, typename H = std::hash<K>> inline auto MapFromLua(lua_State *state, int index) -> Result<std::unordered_map<K, V, H>> {
  if (!lua_istable(state, index)) {
    return Error::Createf("Expected a table at index {}", index);
  }

  std::unordered_map<K, V, H> result;
  lua_pushnil(state); // First key
  while (lua_next(state, index) != 0) {
    auto keyResult = ConvertLuaKey<K>(state, -2);
    if (!keyResult) {
      return keyResult.Error();
    }
    auto valueResult = ConvertLuaValue<V>(state, -1);
    if (!valueResult) {
      return valueResult.Error();
    }
    result.emplace(std::move(keyResult.Value()), std::move(valueResult.Value()));
    lua_pop(state, 1); // Remove value, keep key for next iteration
  }
  return result;
}

// Variant: PushMap with custom conversion
template <typename K, typename V, typename H = std::hash<K>, typename KeyToLuaFn, typename ValueToLuaFn>
auto PushMap(lua_State *state, const std::unordered_map<K, V, H> &value, KeyToLuaFn keyToLua, ValueToLuaFn valueToLua) -> void {
  lua_newtable(state);
  for (const auto &pair : value) {
    keyToLua(state, pair.first);
    valueToLua(state, pair.second);
    lua_rawset(state, -3);
  }
}

// Variant: MapFromLua with custom conversion
template <typename K, typename V, typename H = std::hash<K>, typename KeyFromLuaFn, typename ValueFromLuaFn>
inline auto MapFromLua(lua_State *state, int index, KeyFromLuaFn keyFromLua, ValueFromLuaFn valueFromLua) -> Result<std::unordered_map<K, V, H>> {
  if (!lua_istable(state, index)) {
    return Error::Createf("Expected a table at index {}", index);
  }
  std::unordered_map<K, V, H> result;
  lua_pushnil(state);
  while (lua_next(state, index) != 0) {
    auto keyResult = keyFromLua(state, -2);
    if (!keyResult) {
      return keyResult.Error();
    }
    auto valueResult = valueFromLua(state, -1);
    if (!valueResult) {
      return valueResult.Error();
    }
    result.emplace(std::move(keyResult.Value()), std::move(valueResult.Value()));
    lua_pop(state, 1);
  }
  return result;
}

} // namespace LuaWrap