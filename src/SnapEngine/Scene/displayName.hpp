#pragma once

#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <lua.h>
#include <string>

namespace Engine {

struct DisplayName {
  std::string Name;

  auto DrawGUI(flecs::entity entity) const -> void;
};

const static Type DisplayNameType = Type("DisplayName");

struct LuaDisplayName : LuaWrap::LuaECSObject {
  static auto GetType() -> const Type * { return &DisplayNameType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override { return LuaDisplayName::GetType(); }

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaComponent DisplayNameComponent;

} // namespace Engine