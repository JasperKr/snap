#pragma once

#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <cstdint>
#include <vector>

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::vector<int32_t> FreedUserdataIndices;

struct Userdata {
  int32_t userdataIndex = 0;

  static auto GetFreeUserdataIndex() -> int32_t;

  static auto FreeUserdataIndex(int32_t index) -> void { FreedUserdataIndices.emplace_back(index); }

  static auto SetUserdata(lua_State *state) -> int;
  static auto GetUserdata(lua_State *state) -> int;

  auto OnRemove() -> void;

  auto DrawGUI(lua_State *state) const -> void;
};

extern const ::LuaWrap::LuaComponent UserdataComponent;

} // namespace Engine