#pragma once

#include "Modules/imageData.hpp"
#include "Wrap/Modules/wrap_bytedata.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Image {

auto wrap_NewImagedata(lua_State *state) -> int;
auto wrap_SetPixel(lua_State *state) -> int;
auto wrap_GetPixel(lua_State *state) -> int;
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ImagedataLib = {
    {"setUInt32", Data::wrap_SetUInt32},   {"getUInt32", Data::wrap_GetUInt32},   {"setInt32", Data::wrap_SetInt32}, {"getInt32", Data::wrap_GetInt32},
    {"setUInt16", Data::wrap_SetUInt16},   {"getUInt16", Data::wrap_GetUInt16},   {"setInt16", Data::wrap_SetInt16}, {"getInt16", Data::wrap_GetInt16},
    {"setUInt8", Data::wrap_SetUInt8},     {"getUInt8", Data::wrap_GetUInt8},     {"setInt8", Data::wrap_SetInt8},   {"getInt8", Data::wrap_GetInt8},
    {"setFloat", Data::wrap_SetFloat},     {"getFloat", Data::wrap_GetFloat},     {"setHalf", Data::wrap_SetHalf},   {"getHalf", Data::wrap_GetHalf},
    {"setPixel", wrap_SetPixel},           {"getPixel", wrap_GetPixel},           {"getWidth", wrap_GetWidth},       {"getHeight", wrap_GetHeight},
    {"getDimensions", wrap_GetDimensions}, {"getFormat", wrap_GetFormat},

    {"getSize", Data::wrap_GetSize},       {"getPointer", Data::wrap_GetPointer},

};

extern "C" inline auto luaopen_imagedata(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Image::ImageData::GetType(),
                           ImagedataLib); // NOLINT

  auto documenter = Bindings::LuaDocumentingStruct("Imagedata");

  constexpr auto &createType = Bindings::LuaDocumentingStruct::CreateType;
  constexpr auto &createVType = Bindings::LuaDocumentingStruct::CreateVectorType;

  auto offsetType = createType("Offset", Bindings::BindingLuaType::Integer);
  auto valueType = createType("Value", Bindings::BindingLuaType::Integer);
  auto valueNumberType = createType("Value", Bindings::BindingLuaType::Number);

  documenter.DocumentCustomMethod("setPixel", "Set the pixel at the given coordinates to the given color",
                                  {createType("X", Bindings::BindingLuaType::Integer), createType("Y", Bindings::BindingLuaType::Integer),
                                   createType("Color", Bindings::BindingLuaType::Vec4)});

  documenter.DocumentCustomMethod("getPixel", "Get the color of the pixel at the given coordinates",
                                  {createType("X", Bindings::BindingLuaType::Integer), createType("Y", Bindings::BindingLuaType::Integer)},
                                  createType("Color", Bindings::BindingLuaType::Vec4));

  documenter.DocumentCustomMethod("getWidth", "Get the width of the image data", {}, createType("Width", Bindings::BindingLuaType::Integer));

  documenter.DocumentCustomMethod("getHeight", "Get the height of the image data", {}, createType("Height", Bindings::BindingLuaType::Integer));

  documenter.DocumentCustomMethod("getDimensions", "Get the dimensions of the image data", {}, createType("Dimensions", Bindings::BindingLuaType::Vec2));

  documenter.DocumentCustomMethod("getFormat", "Get the format of the image data", {}, createType("Format", Bindings::BindingLuaType::String));

  // raw data access methods

  documenter.DocumentCustomMethod("setUInt32", "Set a 32-bit unsigned integer at the given byte-offset", {offsetType, valueType});

  documenter.DocumentCustomMethod("getUInt32", "Get a 32-bit unsigned integer at the given byte-offset", {offsetType}, valueType);

  documenter.DocumentCustomMethod("setInt32", "Set a 32-bit signed integer at the given byte-offset", {offsetType, valueType});

  documenter.DocumentCustomMethod("getInt32", "Get a 32-bit signed integer at the given byte-offset", {offsetType}, valueType);

  documenter.DocumentCustomMethod("setUInt16", "Set a 16-bit unsigned integer at the given byte-offset", {offsetType, valueType});

  documenter.DocumentCustomMethod("getUInt16", "Get a 16-bit unsigned integer at the given byte-offset", {offsetType}, valueType);

  documenter.DocumentCustomMethod("setInt16", "Set a 16-bit signed integer at the given byte-offset", {offsetType, valueType});

  documenter.DocumentCustomMethod("getInt16", "Get a 16-bit signed integer at the given byte-offset", {offsetType}, valueType);

  documenter.DocumentCustomMethod("setUInt8", "Set an 8-bit unsigned integer at the given byte-offset", {offsetType, valueType});

  documenter.DocumentCustomMethod("getUInt8", "Get an 8-bit unsigned integer at the given byte-offset", {offsetType}, valueType);

  documenter.DocumentCustomMethod("setInt8", "Set an 8-bit signed integer at the given byte-offset", {offsetType, valueType});

  documenter.DocumentCustomMethod("getInt8", "Get an 8-bit signed integer at the given byte-offset", {offsetType}, valueType);

  documenter.DocumentCustomMethod("setFloat", "Set a 32-bit float at the given byte-offset", {offsetType, valueNumberType});

  documenter.DocumentCustomMethod("getFloat", "Get a 32-bit float at the given byte-offset", {offsetType}, valueNumberType);

  documenter.DocumentCustomMethod("setHalf", "Set a 16-bit float at the given byte-offset", {offsetType, valueNumberType});

  documenter.DocumentCustomMethod("getHalf", "Get a 16-bit float at the given byte-offset", {offsetType}, valueNumberType);

  documenter.Register();

  return 1;
}

} // namespace Wrap::Image