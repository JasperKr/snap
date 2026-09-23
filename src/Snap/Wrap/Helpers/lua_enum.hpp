#pragma once

#include "Modules/error.hpp"
#include "lua.hpp"
#include <lua.h>
#include <unordered_map>
#include <vector>

// Enum wrapper

namespace LuaWrap {

template <typename T>
inline auto EnumFromLua(lua_State *state, int index,
                        std::unordered_map<std::string, T> enumMap)
    -> Result<T> {

  if (!lua_isstring(state, index)) {
    return Error::Unexpected("Expected string for enum value.");
  }

  const char *str = luaL_checkstring(state, index);
  auto iter = enumMap.find(str);
  if (iter == enumMap.end()) {
    std::vector<std::string> validKeys;
    for (const auto &[key, _] : enumMap) {
      validKeys.emplace_back(key);
    }

    std::string validKeysStr;
    for (size_t i = 0; i < validKeys.size(); ++i) {
      validKeysStr += validKeys[i];
      if (i < validKeys.size() - 1) {
        validKeysStr += ", ";
      }
    }

    return Error::Unexpectedf("Invalid enum value: '{}'\nExpected one of: {}",
                              str, validKeysStr);
  }
  return iter->second;
}

template <typename T>
inline auto EnumToLua(lua_State *state, T value,
                      const std::unordered_map<std::string, T> &enumMap)
    -> Error {
  for (const auto &[key, val] : enumMap) {
    if (val == value) {
      lua_pushstring(state, key.c_str());
      return Error::Success();
    }
  }

  return Error::Createf("Invalid enum value: {}", static_cast<int>(value));
}

template <typename T>
inline auto EnumToLua(lua_State *state, T value,
                      const std::unordered_map<T, std::string> &reverseEnumMap)
    -> Error {
  auto iter = reverseEnumMap.find(value);
  if (iter == reverseEnumMap.end()) {
    auto intValue = static_cast<int>(value);
    return Error::Createf("Invalid enum value: {}", intValue);
  }

  lua_pushstring(state, iter->second.c_str());
  return Error::Success();
}

struct LuaEnumBase {
  std::string name;
  std::vector<std::string> options;
};

// NOLINTNEXTLINE, registered enums for generating lua definitions
inline std::vector<LuaEnumBase> RegisteredEnums;

template <typename T> struct LuaEnum : public LuaEnumBase {
private:
  std::unordered_map<std::string, T> EnumMap;
  std::unordered_map<T, std::string> ReverseEnumMap;

public:
  explicit LuaEnum(const std::string &enumName,
                   const std::vector<std::pair<std::string, T>> &entries)
      : LuaEnumBase{enumName, {}} {

    for (const auto &[key, value] : entries) {
      EnumMap[key] = value;
      ReverseEnumMap[value] = key;
      options.emplace_back(key);
    }

    RegisteredEnums.emplace_back(*this);
  }

  auto FromLua(lua_State *state, int index) const -> Result<T> {
    return EnumFromLua(state, index, EnumMap);
  }

  auto ToLua(lua_State *state, T value) const -> Error {
    return EnumToLua<T>(state, value, ReverseEnumMap);
  }

  auto FromLua(lua_State *state, int index, T defaultValue) const -> T {
    if (lua_type(state, index) != LUA_TSTRING) {
      return defaultValue;
    }
    auto result = EnumFromLua(state, index, EnumMap);
    if (Error::IsError(result)) {
      return defaultValue;
    }
    return result.value();
  }

  auto ToLua(lua_State *state, T value, T defaultValue) const -> Error {
    if (value == defaultValue) {
      lua_pushnil(state);
      return Error::Success();
    }
    return EnumToLua(state, value, ReverseEnumMap);
  }

  auto ToString(T value) const -> std::string {
    auto iter = ReverseEnumMap.find(value);
    if (iter == ReverseEnumMap.end()) {
      return "Invalid";
    }
    return iter->second;
  }

  auto FromString(const std::string &str, T defaultValue) const -> T {
    auto iter = EnumMap.find(str);
    if (iter == EnumMap.end()) {
      return defaultValue;
    }
    return iter->second;
  }

  [[nodiscard]] auto GetOptions() const
      -> std::vector<std::pair<std::string, T>> {
    std::vector<std::pair<std::string, T>> options;
    for (const auto &[key, value] : EnumMap) {
      options.emplace_back(key, value);
    }
    return options;
  }
};

} // namespace LuaWrap