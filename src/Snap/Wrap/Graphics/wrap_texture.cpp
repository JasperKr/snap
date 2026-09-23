#include "Wrap/Graphics/wrap_texture.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap.hpp"
#include <imgui.h>
#include <lua.h>
#include <string>
#include <unordered_set>

#include "vulkan/vulkan_core.h"
#include <lua.hpp>
namespace Wrap::Graphics::Texture {
auto inline StringToVkFilter(const char *filterStr) -> VkFilter {
  if (strcmp(filterStr, "nearest") == 0) {
    return VK_FILTER_NEAREST;
  }
  if (strcmp(filterStr, "linear") == 0) {
    return VK_FILTER_LINEAR;
  }
  return VK_FILTER_LINEAR; // Default
}

auto wrap_SetFilter(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  const char *minFilterStr = luaL_checkstring(state, 2);
  const char *magFilterStr = luaL_checkstring(state, 3);
  const char *mipFilterStr = luaL_checkstring(state, 4);

  VkFilter minFilter = StringToVkFilter(minFilterStr);
  VkFilter magFilter = StringToVkFilter(magFilterStr);
  VkFilter mipFilter_ = StringToVkFilter(mipFilterStr);
  auto mipFilter = (mipFilter_ == VK_FILTER_NEAREST)
                       ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                       : VK_SAMPLER_MIPMAP_MODE_LINEAR;

  texture->SetFilter(minFilter, magFilter, mipFilter);

  return 0;
}

auto wrap_GetFilter(lua_State *state) -> int {
  PrintWarning("Getting texture filter");
  PrintWarning("Texture type ptr: {}", (void *)::Graphics::Texture::GetType());
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  VkFilter minFilter = VK_FILTER_LINEAR;
  VkFilter magFilter = VK_FILTER_LINEAR;
  VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  std::tie(minFilter, magFilter, mipFilter) = texture->GetFilter();

  const char *minFilterStr =
      (minFilter == VK_FILTER_NEAREST) ? "nearest" : "linear";
  const char *magFilterStr =
      (magFilter == VK_FILTER_NEAREST) ? "nearest" : "linear";
  const char *mipFilterStr =
      (mipFilter == VK_SAMPLER_MIPMAP_MODE_NEAREST) ? "nearest" : "linear";

  lua_pushstring(state, minFilterStr);
  lua_pushstring(state, magFilterStr);
  lua_pushstring(state, mipFilterStr);

  return 3;
}

auto wrap_SetAnisotropy(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  auto anisotropy = static_cast<float>(luaL_checknumber(state, 2));

  texture->SetAnisotropy(anisotropy);

  return 0;
}

auto wrap_GetAnisotropy(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  float anisotropy = texture->GetAnisotropy();

  lua_pushnumber(state, static_cast<lua_Number>(anisotropy));
  return 1;
}

auto inline StringToAddressMode(const char *addressModeStr)
    -> VkSamplerAddressMode {
  if (strcmp(addressModeStr, "repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
  if (strcmp(addressModeStr, "mirrored_repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  if (strcmp(addressModeStr, "clamp_to_edge") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default
}

auto wrap_SetWrapmode(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  const char *addressModeUStr = luaL_checkstring(state, 2);
  const char *addressModeVStr = luaL_checkstring(state, 3);
  const char *addressModeWStr = luaL_checkstring(state, 4);

  VkSamplerAddressMode addressModeU = StringToAddressMode(addressModeUStr);
  VkSamplerAddressMode addressModeV = StringToAddressMode(addressModeVStr);
  VkSamplerAddressMode addressModeW = StringToAddressMode(addressModeWStr);

  texture->SetWrapmode(addressModeU, addressModeV, addressModeW);

  return 0;
}

auto inline AddressModeToString(VkSamplerAddressMode addressMode) -> const
    char * {
  switch (addressMode) {
  case VK_SAMPLER_ADDRESS_MODE_REPEAT:
    return "repeat";
  case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
    return "mirrored_repeat";
  case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
    return "clamp_to_edge";
  default:
    return "repeat";
  }
}

auto wrap_GetWrapmode(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  std::tie(addressModeU, addressModeV, addressModeW) = texture->GetWrap();

  const char *addressModeUStr = AddressModeToString(addressModeU);
  const char *addressModeVStr = AddressModeToString(addressModeV);
  const char *addressModeWStr = AddressModeToString(addressModeW);

  lua_pushstring(state, addressModeUStr);
  lua_pushstring(state, addressModeVStr);
  lua_pushstring(state, addressModeWStr);

  return 3;
}

auto wrap_SetLodBias(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  auto mipLodBias = static_cast<float>(luaL_checknumber(state, 2));

  texture->SetLodBias(mipLodBias);

  return 0;
}

auto wrap_GetLodBias(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  float mipLodBias = texture->GetLodBias();

  lua_pushnumber(state, static_cast<lua_Number>(mipLodBias));
  return 1;
}

auto wrap_SetLodRange(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  auto minLod = static_cast<float>(luaL_checknumber(state, 2));
  auto maxLod = static_cast<float>(luaL_checknumber(state, 3));

  texture->SetLodRange(minLod, maxLod);

  return 0;
}

auto wrap_GetLodRange(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  float minLod = 0.0F;
  float maxLod = 0.0F;
  std::tie(minLod, maxLod) = texture->GetLodRange();

  lua_pushnumber(state, static_cast<lua_Number>(minLod));
  lua_pushnumber(state, static_cast<lua_Number>(maxLod));
  return 2;
}

inline auto StringToCompareOp(const char *compareOpStr) -> VkCompareOp {
  if (strcmp(compareOpStr, "never") == 0) {
    return VK_COMPARE_OP_NEVER;
  }
  if (strcmp(compareOpStr, "less") == 0) {
    return VK_COMPARE_OP_LESS;
  }
  if (strcmp(compareOpStr, "equal") == 0) {
    return VK_COMPARE_OP_EQUAL;
  }
  if (strcmp(compareOpStr, "lequal") == 0) {
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  }
  if (strcmp(compareOpStr, "greater") == 0) {
    return VK_COMPARE_OP_GREATER;
  }
  if (strcmp(compareOpStr, "notequal") == 0) {
    return VK_COMPARE_OP_NOT_EQUAL;
  }
  if (strcmp(compareOpStr, "gequal") == 0) {
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  }
  if (strcmp(compareOpStr, "always") == 0) {
    return VK_COMPARE_OP_ALWAYS;
  }
  return VK_COMPARE_OP_ALWAYS; // Default
}

inline auto CompareOpToString(VkCompareOp compareOp) -> const char * {
  switch (compareOp) {
  case VK_COMPARE_OP_NEVER:
    return "never";
  case VK_COMPARE_OP_LESS:
    return "less";
  case VK_COMPARE_OP_EQUAL:
    return "equal";
  case VK_COMPARE_OP_LESS_OR_EQUAL:
    return "lequal";
  case VK_COMPARE_OP_GREATER:
    return "greater";
  case VK_COMPARE_OP_NOT_EQUAL:
    return "notequal";
  case VK_COMPARE_OP_GREATER_OR_EQUAL:
    return "gequal";
  case VK_COMPARE_OP_ALWAYS:
  default:
    return "always";
  }
}

auto wrap_SetDepthCompare(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  if (lua_isnoneornil(state, 2) != 0) {
    texture->SetDepthCompare(false, VK_COMPARE_OP_ALWAYS);
    return 0;
  }

  auto compareOp = StringToCompareOp(luaL_checkstring(state, 2));
  auto enable = compareOp != VK_COMPARE_OP_ALWAYS;
  texture->SetDepthCompare(enable, compareOp);

  return 0;
}

auto wrap_GetDepthCompare(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  bool enable = false;
  VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
  std::tie(enable, compareOp) = texture->GetDepthCompare();

  lua_pushstring(state, CompareOpToString(compareOp));
  return 1;
}

auto wrap_GetWidth(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  uint32_t width = texture->GetWidth();

  lua_pushinteger(state, static_cast<lua_Integer>(width));
  return 1;
}

auto wrap_GetHeight(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  uint32_t height = texture->GetHeight();

  lua_pushinteger(state, static_cast<lua_Integer>(height));
  return 1;
}

auto wrap_GetDepth(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  uint32_t depth = texture->GetDepth();

  lua_pushinteger(state, static_cast<lua_Integer>(depth));
  return 1;
}

auto wrap_GetDimensions(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  VkExtent2D dimensions = texture->GetDimensions();

  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.width));
  lua_pushinteger(state, static_cast<lua_Integer>(dimensions.height));
  return 2;
}

auto wrap_GetMipmapCount(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  size_t mipmapCount = texture->GetMipmapCount();

  lua_pushinteger(state, static_cast<lua_Integer>(mipmapCount));
  return 1;
}

auto wrap_GetFormat(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  if (texture == nullptr) {
    return luaL_error(state, "Expected Texture as first argument");
  }

  VkFormat format = texture->GetFormat();

  lua_pushstring(state, ::Graphics::Format::ImageFormatToString(format).data());
  return 1;
}

enum class LuaTextureUsage : uint8_t {
  None = 0,
  Sampled = 1U << 0U,
  RenderTarget = 1U << 1U,
  Storage = 1U << 2U,
};

static inline auto TextureUsageToVkImageUsage(VkFormat format,
                                              LuaTextureUsage usage)
    -> VkImageUsageFlags {
  VkImageUsageFlags flags = 0;
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(LuaTextureUsage::Sampled)) != 0) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(LuaTextureUsage::RenderTarget)) != 0) {
    if (Image::IsDepthOrStencilTexture(format)) {
      flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    } else {
      flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
  }
  if ((static_cast<uint8_t>(usage) &
       static_cast<uint8_t>(LuaTextureUsage::Storage)) != 0) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  return flags;
}

const std::unordered_set<std::string> ValidOptionKeys = {
    "type",    "format",      "mipmaps",     "sampler", "rendertarget",
    "storage", "mipmapcount", "mipmapstart", "linear",
};

auto CheckTopOfStackTableKeys(lua_State *state, int index) -> Error {
  luaL_checktype(state, index, LUA_TTABLE);

  lua_pushnil(state); // first key
  while (lua_next(state, index) != 0) {
    // key at -2, value at -1
    if (lua_type(state, -2) == LUA_TSTRING) {
      const char *keyStr = luaL_checkstring(state, -2);
      if (!ValidOptionKeys.contains(std::string(keyStr))) {
        return Error::Createf("Invalid texture option key: {}", keyStr);
      }
    }

    lua_pop(state, 1); // pop value, keep key for next iteration
  }

  return {};
}

const LuaWrap::LuaEnum<::Graphics::TextureType>
    LuaTextureTypeEnum("TextureType",
                       {
                           {"2D", ::Graphics::TextureType::DEFAULT},
                           {"cube", ::Graphics::TextureType::CUBEMAP},
                           {"3D", ::Graphics::TextureType::VOLUME},
                           {"array", ::Graphics::TextureType::ARRAY},
                       });

// Options:
// { type = "2D"|"3D"|"array"|"cube", format = f, mipmaps = bool, usage = int, mipmapcount = n, mipmapstart = n, linear = bool }
struct LuaOptions {
  ::Graphics::TextureType type = ::Graphics::TextureType::DEFAULT;
  VkFormat format = ::Graphics::DefaultPixelFormat;
  ::Graphics::TextureMipmapOption mipmaps = {};
  LuaTextureUsage usage = LuaTextureUsage::Sampled;
  uint32_t mipmapCount = 0;
  uint32_t mipmapStart = 0;
  bool linear = true;

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  static auto Create(lua_State *state, int index) -> Result<LuaOptions> {
    LuaOptions options{};
    luaL_checktype(state, index, LUA_TTABLE);

    CHECK_ERR(CheckTopOfStackTableKeys(state, index));

    // type
    lua_getfield(state, index, "type");
    if (lua_isnoneornil(state, -1) == 0) {
      auto error = LuaTextureTypeEnum.FromLua(state, -1);
      if (Error::IsError(error)) {
        return Error::Unexpected(
            std::format("Invalid texture type: {}", error.error().message));
      }
      options.type = error.value();
    }
    lua_pop(state, 1);

    // mipmaps
    lua_getfield(state, index, "mipmaps");
    // options.mipmaps = luaL_opt(state, lua_toboolean, -1, 0) != 0;
    if (lua_type(state, -1) == LUA_TSTRING) {
      ::Graphics::TextureMipmapOption mipmapOption =
          ::Graphics::TextureMipmapOption::None;
      const char *mipmapOptionStr = lua_tostring(state, -1);
      if (strcmp(mipmapOptionStr, "none") == 0) {
        mipmapOption = ::Graphics::TextureMipmapOption::None;
      } else if (strcmp(mipmapOptionStr, "init") == 0) {
        mipmapOption = ::Graphics::TextureMipmapOption::Init;
      } else if (strcmp(mipmapOptionStr, "manual") == 0) {
        mipmapOption = ::Graphics::TextureMipmapOption::Manual;
      } else {
        return Error::Unexpected(
            std::format("Invalid mipmaps option: {}", mipmapOptionStr));
      }
      options.mipmaps = mipmapOption;
    }
    lua_pop(state, 1);

    LuaTextureUsage newUsage{};
    bool usageSet = false;

    // sampler
    lua_getfield(state, index, "sampler");
    if (lua_isboolean(state, -1) != 0) {
      if (lua_toboolean(state, -1) != 0) {
        newUsage = static_cast<LuaTextureUsage>(
            static_cast<uint8_t>(newUsage) |
            static_cast<uint8_t>(LuaTextureUsage::Sampled));
      }

      usageSet = true;
    }
    lua_pop(state, 1);

    // rendertarget
    lua_getfield(state, index, "rendertarget");
    if (lua_isboolean(state, -1) != 0) {
      if (lua_toboolean(state, -1) != 0) {
        newUsage = static_cast<LuaTextureUsage>(
            static_cast<uint8_t>(newUsage) |
            static_cast<uint8_t>(LuaTextureUsage::RenderTarget));
      }

      usageSet = true;
    }
    lua_pop(state, 1);

    // storage
    lua_getfield(state, index, "storage");
    if (lua_isboolean(state, -1) != 0) {
      if (lua_toboolean(state, -1) != 0) {
        newUsage = static_cast<LuaTextureUsage>(
            static_cast<uint8_t>(newUsage) |
            static_cast<uint8_t>(LuaTextureUsage::Storage));
      }

      usageSet = true;
    }
    lua_pop(state, 1);

    if (usageSet) {
      if (newUsage == LuaTextureUsage::None) {
        return Error::Unexpected("Texture usage cannot be none.");
      }

      options.usage = newUsage;
    }

    // mipmapcount
    lua_getfield(state, index, "mipmapcount");
    options.mipmapCount = static_cast<uint32_t>(luaL_optinteger(state, -1, 1));
    lua_pop(state, 1);

    // mipmapstart
    lua_getfield(state, index, "mipmapstart");
    options.mipmapStart = luaL_optinteger(state, -1, 0);
    lua_pop(state, 1);

    // linear
    lua_getfield(state, index, "linear");
    options.linear = luaL_opt(state, lua_toboolean, -1, 1) != 0;
    lua_pop(state, 1);

    // format
    lua_getfield(state, index, "format");
    if (lua_isstring(state, -1) != 0) {
      const char *formatStr = luaL_checkstring(state, -1);
      options.format =
          ::Graphics::Format::StringToImageFormat(std::string(formatStr));
    } else {
      options.format = ::Graphics::DefaultPixelFormat;

      if ((static_cast<uint8_t>(options.usage) &
           static_cast<uint8_t>(LuaTextureUsage::Storage)) != 0) {
        options.format = VK_FORMAT_R8G8B8A8_UNORM;
      }
    }
    lua_pop(state, 1);

    return options;
  }
};

static inline auto TextureFromImagedata(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto imageData =
      CHECK_NULL(LuaWrap::ObjectFromLua<Image::ImageData>(state, 1));

  return CHECK_RES(::Graphics::Texture::FromMemory(*ctx, *imageData,
                                                   VK_IMAGE_USAGE_SAMPLED_BIT));
}

static inline auto TextureFromFilepath(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = CHECK_NULL(::Graphics::GetCurrentGraphicsContext());
  const char *filepath = luaL_checkstring(state, 1);

  return ::Graphics::Texture::FromFile(*ctx, filepath,
                                       VK_IMAGE_USAGE_SAMPLED_BIT);
}

static inline auto TextureFromImagedataAndOptions(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = CHECK_NULL(::Graphics::GetCurrentGraphicsContext());
  auto imageData =
      CHECK_NULL(LuaWrap::ObjectFromLua<Image::ImageData>(state, 1));
  auto options = CHECK_RES(LuaOptions::Create(state, 2));

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);
  return ::Graphics::Texture::FromMemory(*ctx, *imageData, usage);
}

static inline auto TextureFromFilepathAndOptions(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  const char *filepath = luaL_checkstring(state, 1);
  auto options = CHECK_RES(LuaOptions::Create(state, 2));

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);
  return ::Graphics::Texture::FromFile(*ctx, filepath, usage, options.mipmaps);
}

static inline auto TextureFromImagedataArrayAndOptions(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  std::vector<Ref<Image::ImageData>> slices;
  size_t len = lua_objlen(state, 1);
  for (size_t i = 0; i < len; ++i) {
    lua_rawgeti(state, 1, static_cast<int>(i + 1));
    auto imageData =
        CHECK_NULL(LuaWrap::ObjectFromLua<Image::ImageData>(state, -1));
    slices.emplace_back(imageData);
    lua_pop(state, 1);
  }

  auto options = CHECK_RES(LuaOptions::Create(state, 2));
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  return ::Graphics::Texture::FromMemory(*ctx, slices, options.type);
}

static inline auto TextureFromWidthAndHeight(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));

  return ::Graphics::Texture::Create(
      *ctx,
      ::Graphics::TextureCreationInfo{
          .size = {width, height},
          .format = ::Graphics::DefaultPixelFormat,
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          .mipmapCount = 1,
          .debugName = "LuaTexture FromWidthAndHeight",
          .textureType = ::Graphics::TextureType::DEFAULT,
      });
}

static inline auto TextureFromWidthHeightAndOptions(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto options = CHECK_RES(LuaOptions::Create(state, 3));

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  int mipmapCount = 1;
  if (options.mipmaps != ::Graphics::TextureMipmapOption::None) {
    mipmapCount = static_cast<int>(Image::GetMipmapCount(width, height));

    if (options.mipmapCount != 0) {
      mipmapCount =
          std::min(mipmapCount, static_cast<int>(options.mipmapCount));
    }
  }

  if (options.mipmapStart != 0) {
    return Error::Unexpected("mipmapstart is not yet implemented.");
  }

  auto texture = CHECK_RES(::Graphics::Texture::Create(
      *ctx, ::Graphics::TextureCreationInfo{
                .size = {width, height},
                .format = options.format,
                .usage = usage,
                .mipmapCount = mipmapCount,
                .debugName = "Lua texture from width, height and options",
                .textureType = options.type,
            }));

  if (options.mipmaps == ::Graphics::TextureMipmapOption::Init) {
    auto &tctx = ::Graphics::GetThreadContext();
    CHECK_ERR(tctx.commandBuffer->MipmapTexture({texture.get()}));
  }

  return texture;
}

static inline auto
TextureFromWidthHeightDepthOrLayersAndOptions(lua_State *state)
    -> Result<Ref<::Graphics::Texture>> {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto width = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  auto height = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto depthOrLayers = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  auto optionsResult = LuaOptions::Create(state, 4);

  if (Error::IsError(optionsResult)) {
    return optionsResult.error();
  }

  LuaOptions options = optionsResult.value();

  auto usage = TextureUsageToVkImageUsage(options.format, options.usage);

  int mipmapCount = 1;
  if (options.mipmaps != ::Graphics::TextureMipmapOption::None) {
    mipmapCount =
        static_cast<int>(Image::GetMipmapCount(width, height, depthOrLayers));

    if (options.mipmapCount != 0) {
      mipmapCount =
          std::min(mipmapCount, static_cast<int>(options.mipmapCount));
    }

    if (options.mipmapStart != 0) {
      return Error::Unexpected("mipmapstart is not yet implemented.");
    }
  }

  auto depth =
      (options.type == ::Graphics::TextureType::VOLUME) ? depthOrLayers : 1;
  auto layers =
      (options.type == ::Graphics::TextureType::ARRAY) ? depthOrLayers : 1;

  auto texture = CHECK_RES(::Graphics::Texture::Create(
      *ctx, ::Graphics::TextureCreationInfo{
                .size = {width, height, depth},
                .arrayLayers = layers,
                .format = options.format,
                .usage = usage,
                .mipmapCount = mipmapCount,
                .debugName =
                    "Lua texture from width, height, depth/layers and options",
                .textureType = options.type,
            }));

  if (options.mipmaps == ::Graphics::TextureMipmapOption::Init) {
    auto &tctx = ::Graphics::GetThreadContext();
    CHECK_ERR(tctx.commandBuffer->MipmapTexture({texture.get()}));
  }

  return texture;
}

// Variants:
// Filepath -> load from file
// Imagedata -> load from image data
// Filepath, Options -> load from file with options
// Imagedata, Options -> load from image data with options
// Array of Imagedata, Options -> load 3D/Array/Cubemap texture
// width, height -> 2D rgba8 1 mip, 1 layer texture
// width, height, Options -> 2D or Cubemap texture
// width, height, depth|layers, Options, -> 3D or Array texture
auto wrap_NewTexture(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  const auto *type = ::Graphics::Texture::GetType();
  int args = lua_gettop(state);

  Result<Ref<::Graphics::Texture>> result;

  if (args == 1) {
    if (LuaWrap::IsType<Image::ImageData>(state, 1)) {
      result = TextureFromImagedata(state);
    } else if (lua_type(state, 1) == LUA_TSTRING) { // Filepath
      result = TextureFromFilepath(state);
    } else {
      return luaL_error(state, "Invalid argument to newTexture");
    }
  } else if (args == 2) {
    // Width, Height or ImageData array + Options or Filepath + Options or ImageData + Options
    if (LuaWrap::IsType<Image::ImageData>(state, 1)) {
      result = TextureFromImagedataAndOptions(state);
    } else if (lua_type(state, 1) == LUA_TSTRING) { // Filepath + Options
      result = TextureFromFilepathAndOptions(state);
    } else if (lua_istable(state, 1) != 0 &&
               lua_istable(state, 2) != 0) { // ImageData array + Options
      result = TextureFromImagedataArrayAndOptions(state);
    } else { // Width, Height
      result = TextureFromWidthAndHeight(state);
    }
  } else if (args == 3) { // width, height, Options
    result = TextureFromWidthHeightAndOptions(state);
  } else if (args == 4) { // width, height, depth|layers, Options
    result = TextureFromWidthHeightDepthOrLayersAndOptions(state);
  } else {
    return luaL_error(state, "Invalid arguments to newTexture");
  }

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to create texture: %s",
                      result.error().message.c_str());
  }
  auto texture = result.value();

  LuaWrap::PushObject(state, type, texture.get());

  return 1;
}

inline auto subresourceRangeFromLua(lua_State *state, int index)
    -> VkImageSubresourceRange {
  VkImageSubresourceRange range{};

  luaL_checktype(state, index, LUA_TTABLE);

  // layerstart
  lua_getfield(state, index, "layerstart");
  range.baseArrayLayer = static_cast<uint32_t>(luaL_optinteger(state, -1, 0));
  lua_pop(state, 1);

  // layercount
  lua_getfield(state, index, "layercount");
  range.layerCount = static_cast<uint32_t>(
      luaL_optinteger(state, -1, VK_REMAINING_ARRAY_LAYERS));
  lua_pop(state, 1);

  // mipmapstart
  lua_getfield(state, index, "mipmapstart");
  range.baseMipLevel = static_cast<uint32_t>(luaL_optinteger(state, -1, 0));
  lua_pop(state, 1);

  // mipmapcount
  lua_getfield(state, index, "mipmapcount");
  range.levelCount = static_cast<uint32_t>(
      luaL_optinteger(state, -1, VK_REMAINING_MIP_LEVELS));
  lua_pop(state, 1);

  return range;
}

// variants
// texture
// texture, options
//
// options: { mipmapstart = n, mipmapcount = n, layerstart = n, layercount = n}
auto wrap_NewTextureView(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  VkImageSubresourceRange range{};
  range.baseArrayLayer = 0;
  range.layerCount = VK_REMAINING_ARRAY_LAYERS;
  range.baseMipLevel = 0;
  range.levelCount = VK_REMAINING_MIP_LEVELS;

  if (lua_gettop(state) > 1) {
    range = subresourceRangeFromLua(state, 2);
  }

  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  auto imageViewType =
      ImageViewTypeEnum.FromLua(state, 3, VK_IMAGE_VIEW_TYPE_2D);

  auto view = LUA_CK_RES(
      ::Graphics::Texture::Create(*ctx, texture.get(), imageViewType, range));

  LuaWrap::PushObject(state, ::Graphics::Texture::GetType(), view.get());

  return 1;
}

auto wrap_GetID(lua_State *state) -> int {
  auto texture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

  LuaWrap::PushPointer(state, texture.get());
  return 1;
}

} // namespace Wrap::Graphics::Texture