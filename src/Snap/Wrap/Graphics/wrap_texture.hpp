#pragma once

#include "Graphics/texture.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vulkan/vulkan_core.h>
namespace Wrap::Graphics::Texture {
auto wrap_SetFilter(lua_State *state) -> int;
auto wrap_GetFilter(lua_State *state) -> int;
auto wrap_SetAnisotropy(lua_State *state) -> int;
auto wrap_GetAnisotropy(lua_State *state) -> int;
auto wrap_SetWrapmode(lua_State *state) -> int;
auto wrap_GetWrapmode(lua_State *state) -> int;
auto wrap_SetBorderColor(lua_State *state) -> int;
auto wrap_GetBorderColor(lua_State *state) -> int;
auto wrap_SetLodBias(lua_State *state) -> int;
auto wrap_GetLodBias(lua_State *state) -> int;
auto wrap_SetLodRange(lua_State *state) -> int;
auto wrap_GetLodRange(lua_State *state) -> int;
auto wrap_SetDepthCompare(lua_State *state) -> int;
auto wrap_GetDepthCompare(lua_State *state) -> int;
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDepth(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
auto wrap_GetMipmapCount(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;
auto wrap_GetID(lua_State *state) -> int; // ImGui texture Identifier
auto wrap_NewTexture(lua_State *state) -> int;
auto wrap_NewTextureView(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

static const LuaWrap::LuaEnum<VkImageViewType> ImageViewTypeEnum{
    "ImageView",
    {
        {"1D", VK_IMAGE_VIEW_TYPE_1D},
        {"2D", VK_IMAGE_VIEW_TYPE_2D},
        {"Volume", VK_IMAGE_VIEW_TYPE_3D},
        {"cubemap", VK_IMAGE_VIEW_TYPE_CUBE},
        {"array1D", VK_IMAGE_VIEW_TYPE_1D_ARRAY},
        {"array2D", VK_IMAGE_VIEW_TYPE_2D_ARRAY},
        {"arrayCube", VK_IMAGE_VIEW_TYPE_CUBE_ARRAY},
    }};

static const LuaWrap::LuaEnum<VkSamplerAddressMode> WrapModeEnum{
    "WrapMode",
    {
        {"repeat", VK_SAMPLER_ADDRESS_MODE_REPEAT},
        {"mirroredrepeat", VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT},
        {"clamp", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
        {"clampborder", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER},
    }};

static const LuaWrap::LuaEnum<VkBorderColor> BorderColorEnum{
    "BorderColor",
    {
        {"transparent", VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK},
        {"black", VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK},
        {"white", VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE},
    }};

static const LuaWrap::LuaEnum<VkFilter> FilterModeEnum{
    "FilterMode",
    {
        {"nearest", VK_FILTER_NEAREST},
        {"linear", VK_FILTER_LINEAR},
        {"cubic", VK_FILTER_CUBIC_EXT},
    }};

static const LuaWrap::LuaEnum<VkSamplerMipmapMode> MipFilterModeEnum{
    "FilterMode",
    {
        {"nearest", VK_SAMPLER_MIPMAP_MODE_NEAREST},
        {"linear", VK_SAMPLER_MIPMAP_MODE_LINEAR},
    }};

static const LuaWrap::LuaEnum<VkCompareOp> CompareOpEnum{
    "CompareMode",
    {
        {"never", VK_COMPARE_OP_NEVER},
        {"less", VK_COMPARE_OP_LESS},
        {"equal", VK_COMPARE_OP_EQUAL},
        {"lequal", VK_COMPARE_OP_LESS_OR_EQUAL},
        {"greater", VK_COMPARE_OP_GREATER},
        {"notequal", VK_COMPARE_OP_NOT_EQUAL},
        {"gequal", VK_COMPARE_OP_GREATER_OR_EQUAL},
        {"always", VK_COMPARE_OP_ALWAYS},
    }};

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> TextureLib = {
    {"setFilter", wrap_SetFilter},
    {"getFilter", wrap_GetFilter},
    {"setAnisotropy", wrap_SetAnisotropy},
    {"getAnisotropy", wrap_GetAnisotropy},
    {"setWrap", wrap_SetWrapmode},
    {"getWrap", wrap_GetWrapmode},
    {"setBorderColor", wrap_SetBorderColor},
    {"getBorderColor", wrap_GetBorderColor},
    {"setLodBias", wrap_SetLodBias},
    {"getLodBias", wrap_GetLodBias},
    {"setLodRange", wrap_SetLodRange},
    {"getLodRange", wrap_GetLodRange},
    {"setDepthCompare", wrap_SetDepthCompare},
    {"getDepthCompare", wrap_GetDepthCompare},
    {"getWidth", wrap_GetWidth},
    {"getHeight", wrap_GetHeight},
    {"getDepth", wrap_GetDepth},
    {"getDimensions", wrap_GetDimensions},
    {"getMipmapCount", wrap_GetMipmapCount},
    {"getFormat", wrap_GetFormat},
    {"getID", wrap_GetID},
};

extern "C" inline auto luaopen_texture(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Graphics::Texture::GetType(),
                           TextureLib); // NOLINT

  return 1;
}

} // namespace Wrap::Graphics::Texture