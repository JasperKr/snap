#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/MeshShape.h"

namespace Engine::Physics {

struct MeshShape {
  JPH::MeshShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type meshShapeType = Type("MeshShape");

struct LuaMeshShape : LuaWrap::LuaECSObject {
  explicit LuaMeshShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &meshShapeType; }
  auto GetInstanceType() const -> const Type * override { return &meshShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaMeshShape> { return Ref<LuaMeshShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent MeshShapeComponent{{
    {"getMass", LuaMeshShape::wrap_GetMass},
    {"getInertiaTensor", LuaMeshShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics