#pragma once

#include "Modules/bindings.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/reflectBindings.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Data {

auto wrap_SetUInt32(lua_State *state) -> int;
auto wrap_SetInt32(lua_State *state) -> int;
auto wrap_SetUInt16(lua_State *state) -> int;
auto wrap_SetInt16(lua_State *state) -> int;
auto wrap_SetUInt8(lua_State *state) -> int;
auto wrap_SetInt8(lua_State *state) -> int;

auto wrap_SetFloat(lua_State *state) -> int;
auto wrap_SetHalf(lua_State *state) -> int;

auto wrap_GetUInt32(lua_State *state) -> int;
auto wrap_GetInt32(lua_State *state) -> int;
auto wrap_GetUInt16(lua_State *state) -> int;
auto wrap_GetInt16(lua_State *state) -> int;
auto wrap_GetUInt8(lua_State *state) -> int;
auto wrap_GetInt8(lua_State *state) -> int;

auto wrap_GetFloat(lua_State *state) -> int;
auto wrap_GetHalf(lua_State *state) -> int;

auto wrap_GetSize(lua_State *state) -> int;
auto wrap_GetPointer(lua_State *state) -> int;

auto wrap_NewBytedata(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> BytedataLib = {
    {"setUInt32", wrap_SetUInt32}, {"getUInt32", wrap_GetUInt32},   {"setInt32", wrap_SetInt32}, {"getInt32", wrap_GetInt32},
    {"setUInt16", wrap_SetUInt16}, {"getUInt16", wrap_GetUInt16},   {"setInt16", wrap_SetInt16}, {"getInt16", wrap_GetInt16},
    {"setUInt8", wrap_SetUInt8},   {"getUInt8", wrap_GetUInt8},     {"setInt8", wrap_SetInt8},   {"getInt8", wrap_GetInt8},
    {"setFloat", wrap_SetFloat},   {"getFloat", wrap_GetFloat},     {"setHalf", wrap_SetHalf},   {"getHalf", wrap_GetHalf},

    {"getSize", wrap_GetSize},     {"getPointer", wrap_GetPointer},

};

extern "C" inline auto luaopen_bytedata(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Data::ByteData::GetType(),
                           BytedataLib); // NOLINT

  auto documenter = Bindings::LuaDocumentingStruct("Bytedata");
  constexpr auto &createType = Bindings::LuaDocumentingStruct::CreateType;
  constexpr auto &createVType = Bindings::LuaDocumentingStruct::CreateVectorType;

  const auto &offsetType = createType("Offset", Bindings::BindingLuaType::Integer);
  const auto &valueType = createType("Value", Bindings::BindingLuaType::Integer);
  const auto &numberValueType = createType("Value", Bindings::BindingLuaType::Number);

  // setUInt32
  documenter.DocumentCustomMethod("setUInt32", "Set a 32-bit unsigned integer at the given byte-offset", {offsetType, valueType});

  // getUInt32
  documenter.DocumentCustomMethod("getUInt32", "Get a 32-bit unsigned integer at the given byte-offset", {offsetType}, valueType);

  // setInt32
  documenter.DocumentCustomMethod("setInt32", "Set a 32-bit signed integer at the given byte-offset", {offsetType, valueType});

  // getInt32
  documenter.DocumentCustomMethod("getInt32", "Get a 32-bit signed integer at the given byte-offset", {offsetType}, valueType);

  // setUInt16
  documenter.DocumentCustomMethod("setUInt16", "Set a 16-bit unsigned integer at the given byte-offset", {offsetType, valueType});

  // getUInt16
  documenter.DocumentCustomMethod("getUInt16", "Get a 16-bit unsigned integer at the given byte-offset", {offsetType}, valueType);

  // setInt16
  documenter.DocumentCustomMethod("setInt16", "Set a 16-bit signed integer at the given byte-offset", {offsetType, valueType});

  // getInt16
  documenter.DocumentCustomMethod("getInt16", "Get a 16-bit signed integer at the given byte-offset", {offsetType}, valueType);

  // setUInt8
  documenter.DocumentCustomMethod("setUInt8", "Set an 8-bit unsigned integer at the given byte-offset", {offsetType, valueType});

  // getUInt8
  documenter.DocumentCustomMethod("getUInt8", "Get an 8-bit unsigned integer at the given byte-offset", {offsetType}, valueType);

  // setInt8
  documenter.DocumentCustomMethod("setInt8", "Set an 8-bit signed integer at the given byte-offset", {offsetType, valueType});

  // getInt8
  documenter.DocumentCustomMethod("getInt8", "Get an 8-bit signed integer at the given byte-offset", {offsetType}, valueType);

  // setFloat
  documenter.DocumentCustomMethod("setFloat", "Set a 32-bit float at the given byte-offset", {offsetType, numberValueType});

  // getFloat
  documenter.DocumentCustomMethod("getFloat", "Get a 32-bit float at the given byte-offset", {offsetType}, numberValueType);

  // setHalf
  documenter.DocumentCustomMethod("setHalf", "Set a 16-bit half-precision float at the given byte-offset", {offsetType, numberValueType});

  // getHalf
  documenter.DocumentCustomMethod("getHalf", "Get a 16-bit half-precision float at the given byte-offset", {offsetType}, numberValueType);

  // getSize
  documenter.DocumentCustomMethod({
      .name = "getSize",
      .description = "Get the size of the byte data in bytes",
      .parameters = {},
      .returnType =
          Bindings::TypeInfo{
              .name = "Size",
              .luaType = Bindings::BindingLuaType::Integer,
          },
  });

  // getPointer
  documenter.DocumentCustomMethod({
      .name = "getPointer",
      .description = "Get the raw pointer to the byte data as a light userdata",
      .parameters = {},
      .returnType =
          Bindings::TypeInfo{
              .name = "Pointer",
              .type = ::Data::ByteData::GetType(),
              .luaType = Bindings::BindingLuaType::Userdata,
          },
  });

  // Vector overloads for setters

  // setUInt32 (vector)
  documenter.DocumentCustomMethod("setUInt32", "Set multiple 32-bit unsigned integers at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Integer)});

  // setInt32 (vector)
  documenter.DocumentCustomMethod("setInt32", "Set multiple 32-bit signed integers at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Integer)});

  // setUInt16 (vector)
  documenter.DocumentCustomMethod("setUInt16", "Set multiple 16-bit unsigned integers at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Integer)});

  // setInt16 (vector)
  documenter.DocumentCustomMethod("setInt16", "Set multiple 16-bit signed integers at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Integer)});

  // setUInt8 (vector)
  documenter.DocumentCustomMethod("setUInt8", "Set multiple 8-bit unsigned integers at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Integer)});

  // setInt8 (vector)
  documenter.DocumentCustomMethod("setInt8", "Set multiple 8-bit signed integers at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Integer)});

  // setFloat (vector)
  documenter.DocumentCustomMethod("setFloat", "Set multiple 32-bit floats at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Number)});

  // setHalf (vector)
  documenter.DocumentCustomMethod("setHalf", "Set multiple 16-bit half-precision floats at the given byte-offset",
                                  {offsetType, createVType("Values", Bindings::BindingLuaType::Number)});

  documenter.Register();

  return 1;
}

} // namespace Wrap::Data