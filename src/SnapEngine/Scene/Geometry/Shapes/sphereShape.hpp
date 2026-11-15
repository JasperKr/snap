#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/SphereShape.h"

namespace Engine::Physics {

struct SphereShape {
  JPH::SphereShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type sphereShapeType = Type("SphereShape");

struct LuaSphereShape : LuaWrap::LuaECSObject {
  explicit LuaSphereShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &sphereShapeType; }
  auto GetInstanceType() const -> const Type * override { return &sphereShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaSphereShape> { return Ref<LuaSphereShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetRadius(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent SphereShapeComponent{{
    {"getRadius", LuaSphereShape::wrap_GetRadius},
    {"getMass", LuaSphereShape::wrap_GetMass},
    {"getInertiaTensor", LuaSphereShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics