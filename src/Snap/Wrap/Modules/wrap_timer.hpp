#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vector>
namespace Wrap::Timer {

auto wrap_GetTime(lua_State *state) -> int;
auto wrap_GetDelta(lua_State *state) -> int;
auto wrap_GetAverageDelta(lua_State *state) -> int;
auto wrap_GetFPS(lua_State *state) -> int;
auto wrap_Sleep(lua_State *state) -> int;
auto wrap_Step(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> TimerLib = {
    {"getTime", wrap_GetTime}, {"getDelta", wrap_GetDelta}, {"getAverageDelta", wrap_GetAverageDelta},
    {"getFPS", wrap_GetFPS},   {"sleep", wrap_Sleep},       {"step", wrap_Step},
};

static const std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_timer(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "timer",
      .Functions = TimerLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,

  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Timer