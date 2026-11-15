#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/TaperedCylinderShape.h"

namespace Engine::Physics {

struct TaperedCylinderShape {
  JPH::TaperedCylinderShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type taperedCylinderShapeType = Type("TaperedCylinderShape");

struct LuaTaperedCylinderShape : LuaWrap::LuaECSObject {
  explicit LuaTaperedCylinderShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &taperedCylinderShapeType; }
  auto GetInstanceType() const -> const Type * override { return &taperedCylinderShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaTaperedCylinderShape> { return Ref<LuaTaperedCylinderShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetRadius(lua_State *state) -> int;
  static auto wrap_GetHalfHeight(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent TaperedCylinderShapeComponent{{
    {"getRadius", LuaTaperedCylinderShape::wrap_GetRadius},
    {"getHalfHeight", LuaTaperedCylinderShape::wrap_GetHalfHeight},
    {"getMass", LuaTaperedCylinderShape::wrap_GetMass},
    {"getInertiaTensor", LuaTaperedCylinderShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics