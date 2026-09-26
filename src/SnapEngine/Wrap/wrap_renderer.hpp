#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vector>

namespace Engine::Renderer {

auto wrap_Initialize(lua_State *state) -> int;
auto wrap_ReloadShaders(lua_State *state) -> int;

static const std::vector<luaL_Reg> RendererLib = {
    {"initialize", wrap_Initialize},
    {"reloadShaders", wrap_ReloadShaders},
};

static const std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_renderer(lua_State *state) -> int {
  auto module = ::LuaWrap::LuaModule{
      .Name = "renderer",
      .Functions = RendererLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Engine::Renderer