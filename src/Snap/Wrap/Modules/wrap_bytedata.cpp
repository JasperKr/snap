#include "wrap_bytedata.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/color.hpp"
#include "Wrap/wrap.hpp"
#include <cstdint>
#include <lua.hpp>

namespace Wrap::Data {

template <typename T> constexpr auto SetIntFromLuaData(lua_State *state) -> int {
  auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Data::ByteData>(state, 1));

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }

  auto top = lua_gettop(state);
  auto *data = reinterpret_cast<T *>(span.data() + offset); // NOLINT
  if (((top - 2) * sizeof(T)) + offset > bytedata->GetSize()) {
    return luaL_error(state, "Offset out of bounds.");
  }
  for (int i = 3; i <= top; ++i) {
    data[i - 3] = static_cast<T>(luaL_checknumber(state, i)); // NOLINT
  }
  return 0;
}

template <typename T> constexpr auto SetFloatFromLuaData(lua_State *state) -> int {
  auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Data::ByteData>(state, 1));

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }

  if (lua_istable(state, 3)) {
    size_t len = lua_objlen(state, 3);

    if ((len * sizeof(T)) + offset > bytedata->GetSize()) {
      return luaL_error(state, "Offset out of bounds.");
    }

    auto data = reinterpret_cast<float *>(span.data() + offset); // NOLINT
    for (int i = 0; i < len; ++i) {
      lua_rawgeti(state, 3, i + 1);
      data[i] = static_cast<T>(luaL_checknumber(state, -1)); // NOLINT
    }
  } else if (lua_isnumber(state, 3) != 0) {
    if (offset + sizeof(T) > bytedata->GetSize()) {
      return luaL_error(state, "Offset out of bounds.");
    }

    auto top = lua_gettop(state);
    auto *data = reinterpret_cast<float *>(span.data() + offset); // NOLINT
    for (int i = 3; i <= top; ++i) {
      data[i - 3] = static_cast<T>(luaL_checknumber(state, i)); // NOLINT
    }

  } else {
    return luaL_error(state, "Invalid value to set.");
  }

  return 0;
}

template <typename T> constexpr auto GetIntToLuaData(lua_State *state) -> int {
  auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Data::ByteData>(state, 1));

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;
  auto count = 1L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }
  if (lua_isnumber(state, 3) != 0) {
    count = luaL_checkinteger(state, 3);
  }
#ifndef NDEBUG
  if (count < 1) {
    return luaL_error(state, "Count must be at least 1.");
  }
  if ((offset + (count * sizeof(T))) > bytedata->GetSize()) {
    return luaL_error(state, "Offset out of bounds.");
  }
  if (offset % sizeof(T) != 0) {
    return luaL_error(state, "Offset must be aligned to data type size.");
  }
#endif

  for (int i = 0; i < count; ++i) {
    T output{};
    std::memcpy(&output, &span[offset + (i * sizeof(T))], sizeof(T));
    lua_pushinteger(state, output);
  }

  return static_cast<int>(count);
}

template <typename T> constexpr auto GetFloatToLuaData(lua_State *state) -> int {
  auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Data::ByteData>(state, 1));

  auto span = bytedata->GetDataSpan<uint8_t>();
  auto offset = 0L;
  auto count = 1L;

  if (lua_isnumber(state, 2) != 0) {
    offset = luaL_checkinteger(state, 2);
  }
  if (lua_isnumber(state, 3) != 0) {
    count = luaL_checkinteger(state, 3);
  }
#ifndef NDEBUG
  if (count < 1) {
    return luaL_error(state, "Count must be at least 1.");
  }
  if ((offset + (count * sizeof(T))) > bytedata->GetSize()) {
    return luaL_error(state, "Offset out of bounds.");
  }
  if (offset % sizeof(T) != 0) {
    return luaL_error(state, "Offset must be aligned to data type size.");
  }
#endif
  for (int i = 0; i < count; ++i) {
    T value{};
    std::memcpy(&value, &span[offset + (i * sizeof(T))], sizeof(T));
    lua_pushnumber(state, value);
  }

  return static_cast<int>(count);
}

// value, offset
// or table, offset
auto wrap_SetUInt32(lua_State *state) -> int { return SetIntFromLuaData<uint32_t>(state); }
auto wrap_SetInt32(lua_State *state) -> int { return SetIntFromLuaData<int32_t>(state); }
auto wrap_SetUInt16(lua_State *state) -> int { return SetIntFromLuaData<uint16_t>(state); }
auto wrap_SetInt16(lua_State *state) -> int { return SetIntFromLuaData<int16_t>(state); }
auto wrap_SetUInt8(lua_State *state) -> int { return SetIntFromLuaData<uint8_t>(state); }
auto wrap_SetInt8(lua_State *state) -> int { return SetIntFromLuaData<int8_t>(state); }

auto wrap_SetFloat(lua_State *state) -> int { return SetFloatFromLuaData<float>(state); }
auto wrap_SetHalf(lua_State *state) -> int { return SetFloatFromLuaData<numeric::float16_t>(state); }

auto wrap_GetUInt32(lua_State *state) -> int { return GetIntToLuaData<uint32_t>(state); }
auto wrap_GetInt32(lua_State *state) -> int { return GetIntToLuaData<int32_t>(state); }
auto wrap_GetUInt16(lua_State *state) -> int { return GetIntToLuaData<uint16_t>(state); }
auto wrap_GetInt16(lua_State *state) -> int { return GetIntToLuaData<int16_t>(state); }
auto wrap_GetUInt8(lua_State *state) -> int { return GetIntToLuaData<uint8_t>(state); }
auto wrap_GetInt8(lua_State *state) -> int { return GetIntToLuaData<int8_t>(state); }

auto wrap_GetFloat(lua_State *state) -> int { return GetFloatToLuaData<float>(state); }
auto wrap_GetHalf(lua_State *state) -> int { return GetFloatToLuaData<numeric::float16_t>(state); }

auto wrap_GetSize(lua_State *state) -> int {
  auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Data::ByteData>(state, 1));

  lua_pushinteger(state, static_cast<lua_Integer>(bytedata->GetSize()));
  return 1;
}
auto wrap_GetPointer(lua_State *state) -> int {
  auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Data::ByteData>(state, 1));

  lua_pushlightuserdata(state, static_cast<void *>(bytedata->GetData()));
  return 1;
}

auto wrap_NewBytedata(lua_State *state) -> int {
  auto size = static_cast<size_t>(luaL_checkinteger(state, 1));
  // auto *bytedata = new ::Data::ByteData(size); // NOLINT
  auto bytedata = Ref<::Data::ByteData>::Make(size);

  LuaWrap::PushObject(state, ::Data::ByteData::GetType(), bytedata.get());
  return 1;
}

} // namespace Wrap::Data