#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/PlaneShape.h"

namespace Engine::Physics {

struct PlaneShape {
  JPH::PlaneShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type planeShapeType = Type("PlaneShape");

struct LuaPlaneShape : LuaWrap::LuaECSObject {
  explicit LuaPlaneShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &planeShapeType; }
  auto GetInstanceType() const -> const Type * override { return &planeShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaPlaneShape> { return Ref<LuaPlaneShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetPlane(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent PlaneShapeComponent{{
    {"getPlane", LuaPlaneShape::wrap_GetPlane},
    {"getMass", LuaPlaneShape::wrap_GetMass},
    {"getInertiaTensor", LuaPlaneShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics