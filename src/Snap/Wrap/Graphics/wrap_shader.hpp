#pragma once

#include "Graphics/shader.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <cstdint>

namespace Wrap::Graphics::Shader {

auto wrap_Send(lua_State *state) -> int;
auto wrap_HasUniform(lua_State *state) -> int;
auto wrap_GetUniforms(lua_State *state) -> int;
auto wrap_GetThreadgroupSize(lua_State *state) -> int;
auto wrap_GetWaveSize(lua_State *state) -> int;

auto wrap_NewShader(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ShaderLib = {
    {"send", wrap_Send},
    {"hasUniform", wrap_HasUniform},
    {"getUniforms", wrap_GetUniforms},
    {"getThreadgroupSize", wrap_GetThreadgroupSize},
    {"getWaveSize", wrap_GetWaveSize},
};

enum class GraphicsResourceUsage : uint64_t {
  // Do note that if this is ever changed, these usages are directly cast to
  // the read/write flag parameters when sending.
  Sampled = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
  Uniform = VK_ACCESS_2_UNIFORM_READ_BIT,
  Read = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
  Write = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
  ReadWrite = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
};

static const ::LuaWrap::LuaEnum<GraphicsResourceUsage>
    GraphicsResourceUsageEnumHelper{
        "GraphicsResourceUsage",
        {
            {"sampled", GraphicsResourceUsage::Sampled},
            {"read", GraphicsResourceUsage::Read},
            {"write", GraphicsResourceUsage::Write},
            {"readwrite", GraphicsResourceUsage::ReadWrite},
        }};

extern "C" inline auto luaopen_shader(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Graphics::Shader::GetType(),
                           ShaderLib); // NOLINT

  return 1;
}
} // namespace Wrap::Graphics::Shader
