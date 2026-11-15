#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/CylinderShape.h"

namespace Engine::Physics {

struct CylinderShape {
  JPH::CylinderShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type cylinderShapeType = Type("CylinderShape");

struct LuaCylinderShape : LuaWrap::LuaECSObject {
  explicit LuaCylinderShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &cylinderShapeType; }
  auto GetInstanceType() const -> const Type * override { return &cylinderShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaCylinderShape> { return Ref<LuaCylinderShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetRadius(lua_State *state) -> int;
  static auto wrap_GetHalfHeight(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent CylinderShapeComponent{{
    {"getRadius", LuaCylinderShape::wrap_GetRadius},
    {"getHalfHeight", LuaCylinderShape::wrap_GetHalfHeight},
    {"getMass", LuaCylinderShape::wrap_GetMass},
    {"getInertiaTensor", LuaCylinderShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics