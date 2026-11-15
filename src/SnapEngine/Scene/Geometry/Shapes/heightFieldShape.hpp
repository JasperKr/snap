#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/HeightFieldShape.h"

namespace Engine::Physics {

struct HeightFieldShape {
  JPH::HeightFieldShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type heightFieldShapeType = Type("HeightFieldShape");

struct LuaHeightFieldShape : LuaWrap::LuaECSObject {
  explicit LuaHeightFieldShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &heightFieldShapeType; }
  auto GetInstanceType() const -> const Type * override { return &heightFieldShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaHeightFieldShape> { return Ref<LuaHeightFieldShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent HeightFieldShapeComponent{{
    {"getMass", LuaHeightFieldShape::wrap_GetMass},
    {"getInertiaTensor", LuaHeightFieldShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics