#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"

namespace Engine::Physics {

struct CapsuleShape {
  JPH::CapsuleShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type capsuleShapeType = Type("CapsuleShape");

struct LuaCapsuleShape : LuaWrap::LuaECSObject {
  explicit LuaCapsuleShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &capsuleShapeType; }
  auto GetInstanceType() const -> const Type * override { return &capsuleShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaCapsuleShape> { return Ref<LuaCapsuleShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetRadius(lua_State *state) -> int;
  static auto wrap_GetHalfHeight(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent CapsuleShapeComponent{{
    {"getRadius", LuaCapsuleShape::wrap_GetRadius},
    {"getHalfHeight", LuaCapsuleShape::wrap_GetHalfHeight},
    {"getMass", LuaCapsuleShape::wrap_GetMass},
    {"getInertiaTensor", LuaCapsuleShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics