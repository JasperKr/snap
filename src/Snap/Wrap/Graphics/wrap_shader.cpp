
#include "Wrap/Graphics/wrap_shader.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/shader.hpp"
#include "Modules/bytedata.hpp"
#include "Wrap/Graphics/wrap_reflection.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_utils.hpp"
#include <bit>
#include <cstdint>
#include <format>
#include <lua.hpp>
namespace Wrap::Graphics::Shader {

using namespace ::Graphics::Reflect;

// TODO: Add externs input support
// Modulename, {name=value, ...}
auto wrap_NewShader(lua_State *state) -> int {
  auto *ctx = LUA_CK_NULL(::Graphics::GetCurrentGraphicsContext());

  const auto *type = ::Graphics::Shader::GetType();
  int args = lua_gettop(state);

  const char *name = luaL_checkstring(state, 1); // module name

  std::string shaderDebugName = "Shader_" + std::string(name);

  if (args == 2 && lua_istable(state, 2)) {
    lua_getfield(state, 2, "name");
    const char *customName = lua_tostring(state, -1);
    lua_pop(state, 1);

    if (customName != nullptr) {
      shaderDebugName = std::string(customName);
    }
  }

  auto shader = LUA_CK_RES(
      ::Graphics::Shader::Create(*ctx, std::string(name), shaderDebugName));

  LuaWrap::PushObject(state, type, shader.get());

  return 1;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto wrap_Send(lua_State *state) -> int {
  auto shader =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Shader>(state, 1));
  auto key = LUA_CK_RES(ResourceKeyFromSingleLuaObject(state, 2));
  const ::Graphics::GraphicsContext &context =
      *::Graphics::GetCurrentGraphicsContext();

  // Base index of 1; + 1 for shader object; + 1 for key part
  auto valueOffset = 3;

  if (LuaWrap::IsType<::Graphics::Texture>(state, valueOffset)) {
    auto texture = LUA_CK_NULL(
        LuaWrap::ObjectFromLua<::Graphics::Texture>(state, valueOffset));

    auto usage = GraphicsResourceUsageEnumHelper.FromLua(
        state, valueOffset + 1, GraphicsResourceUsage::Sampled);

    LUA_ASSERT(usage != GraphicsResourceUsage::Uniform);

    if (usage == GraphicsResourceUsage::Sampled) {
      LUA_CK_ERR(
          shader->Send(context, key, ::Graphics::SampledTexture{texture}));
    } else {
      LUA_CK_ERR(shader->Send(
          context, key, ::Graphics::StorageTexture(texture, (uint64_t)usage)));
    }

  } else if (LuaWrap::IsType<::Graphics::StructuredBuffer>(state,
                                                           valueOffset)) {
    auto buffer =
        LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(
            state, valueOffset));
    auto usage = GraphicsResourceUsageEnumHelper.FromLua(
        state, valueOffset + 1, GraphicsResourceUsage::Uniform);

    LUA_ASSERT(usage != GraphicsResourceUsage::Sampled);

    if (usage == GraphicsResourceUsage::Uniform) {
      LUA_CK_ERR(shader->Send(context, key, ::Graphics::UniformBuffer{buffer}));
    } else {
      LUA_CK_ERR(shader->Send(
          context, key, ::Graphics::StorageBuffer(buffer, (uint64_t)usage)));
    }
  } else if (lua_type(state, valueOffset) == LUA_TNUMBER) {
    auto varargsCount = lua_gettop(state) - valueOffset + 1;
    std::vector<uint8_t> data{};

    data.reserve(sizeof(uint32_t) * static_cast<size_t>(varargsCount));

    const auto *uniformInfo = shader->GetUniform(key);
    if (uniformInfo == nullptr) {
      return luaL_error(state, "Uniform `%s` not found.",
                        ResourceKeyToString(key).c_str());
    }
    if (!(uniformInfo->IsScalar() || uniformInfo->IsVector() ||
          uniformInfo->IsMatrix())) {
      return luaL_error(
          state, "Unable to send uniform `%s`: expected %s, got number",
          ResourceKeyToString(key).c_str(), uniformInfo->GetTypename().data());
    }

    ScalarType scalarType{};

    if (uniformInfo->Is<ScalarInfo>()) {
      scalarType = uniformInfo->GetInfo<ScalarInfo>().type;
    } else if (uniformInfo->Is<VectorInfo>()) {
      scalarType = uniformInfo->GetInfo<VectorInfo>().scalarType;
    } else if (uniformInfo->Is<MatrixInfo>()) {
      scalarType = ScalarType::Float;
    }

    LUA_ASSERT_MSG(
        scalarType != ScalarType::Unknown,
        std::format("Unable to send uniform `{}`: unknown scalar type",
                    ResourceKeyToString(key))
            .c_str());

    for (int i = 0; i < varargsCount; ++i) {
      LUA_CK_ERR(
          Wrap::Utils::SetData(lua_tonumber(state, valueOffset + i),
                               data.data() + (i * sizeof(uint32_t)), // NOLINT
                               scalarType));
    }

    auto span = std::span<uint8_t>( // NOLINTNEXTLINE reinterpret cast
        reinterpret_cast<uint8_t *>(data.data()),
        sizeof(uint32_t) * static_cast<size_t>(varargsCount));

    LUA_CK_ERR(shader->Send(key, span));
  } else if (lua_type(state, valueOffset) == LUA_TTABLE) {
    std::vector<uint32_t> data;
    uint64_t tableLength = lua_objlen(state, valueOffset);
    data.reserve(sizeof(uint32_t) * static_cast<size_t>(tableLength));

    const auto *info = shader->GetUniform(key);
    if (info == nullptr) {
      return luaL_error(state, "Uniform `%s` not found.",
                        ResourceKeyToString(key).c_str());
    }

    for (uint64_t i = 0; i < tableLength; ++i) {
      lua_rawgeti(state, valueOffset, static_cast<int>(i + 1));
      auto scalarType = ScalarType::Unknown;
      if (info->IsVector()) {
        const auto &vectorInfo = info->GetInfo<VectorInfo>();
        scalarType = vectorInfo.scalarType;
      } else if (info->IsMatrix()) {
        scalarType = ScalarType::Float;
      } else if (info->IsScalar()) {
        scalarType = info->GetInfo<ScalarInfo>().type;
      } else {
        return luaL_error(state,
                          "Unable to send uniform `%s`: expected vector, "
                          "matrix, or scalar, got %s",
                          ResourceKeyToString(key).c_str(),
                          luaL_typename(state, -1));
      }

      switch (scalarType) {
      case ScalarType::Unknown:
        return luaL_error(state,
                          "Unable to send uniform `%s`: unknown scalar type",
                          ResourceKeyToString(key).c_str());
      case ScalarType::Float:
        data.emplace_back(std::bit_cast<uint32_t>(
            static_cast<float>(lua_tonumber(state, -1)))); // NOLINT
        break;
      case ScalarType::Int:
      case ScalarType::UInt:
        data.emplace_back(
            static_cast<uint32_t>(lua_tointeger(state, -1))); // NOLINT
        break;
      case ScalarType::Bool:
        data.emplace_back(lua_toboolean(state, -1));
        break;
      }

      lua_pop(state, 1);
    }

    auto span = std::span<uint8_t>( // NOLINTNEXTLINE reinterpret cast
        reinterpret_cast<uint8_t *>(data.data()),
        sizeof(uint32_t) * static_cast<size_t>(tableLength));

    LUA_CK_ERR(shader->Send(key, span));
  } else if (LuaWrap::IsType<Data::ByteData>(state, valueOffset)) {
    auto byteData =
        LUA_CK_NULL(LuaWrap::ObjectFromLua<Data::ByteData>(state, valueOffset));
    auto span = byteData->GetDataSpan();

    LUA_CK_ERR(shader->Send(key, span));
  } else {
    const auto *resourceInfo =
        LUA_CK_NULL_MSG(shader->GetUniform(key), "Uniform not found");

    return luaL_error(state, "Unable to send uniform `%s`: expected %s, got %s",
                      ResourceKeyToString(key).c_str(),
                      resourceInfo->GetTypename().data(),
                      luaL_typename(state, valueOffset));
  }

  return 0;
}

auto wrap_HasUniform(lua_State *state) -> int {
  auto shader =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Shader>(state, 1));

  auto key = LUA_CK_RES(ResourceKeyFromSingleLuaObject(state, 2));

  bool hasUniform = shader->GetUniform(key) != nullptr;
  lua_pushboolean(state, static_cast<int>(hasUniform));
  return 1;
}
auto wrap_GetUniforms(lua_State *state) -> int {
  auto shader =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Shader>(state, 1));

  return 0;
}
auto wrap_GetThreadgroupSize(lua_State *state) -> int {
  auto shader =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Shader>(state, 1));

  auto threadgroupSize = LUA_CK_RES(shader->GetThreadgroupSize());

  lua_pushinteger(state, static_cast<lua_Integer>(threadgroupSize.x));
  lua_pushinteger(state, static_cast<lua_Integer>(threadgroupSize.y));
  lua_pushinteger(state, static_cast<lua_Integer>(threadgroupSize.z));
  return 3;
}

auto wrap_GetWaveSize(lua_State *state) -> int {
  auto shader =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Shader>(state, 1));

  auto waveSize = shader->GetWaveSize();

  lua_pushinteger(state, static_cast<lua_Integer>(waveSize));
  return 1;
}

} // namespace Wrap::Graphics::Shader