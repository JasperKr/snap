#pragma once

#include "Graphics/snapshot.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Graphics::Snapshot {

// TODO: At some point i might want to read snapshot data from Lua
auto wrap_Draw(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> SnapshotLib = {
    {"draw", wrap_Draw},
};

extern "C" inline auto luaopen_snapshot(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Graphics::Snapshot::ThreadSnapshot::GetType(),
                           SnapshotLib); // NOLINT

  return 1;
}
} // namespace Wrap::Graphics::Snapshot
