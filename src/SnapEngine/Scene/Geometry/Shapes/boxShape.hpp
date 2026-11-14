#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/BoxShape.h"

namespace Engine::Physics {

struct BoxCollider {
  JPH::BoxShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type boxColliderType = Type("BoxCollider");

struct LuaBoxShape : LuaWrap::LuaECSObject {
  explicit LuaBoxShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &boxColliderType; }
  auto GetInstanceType() const -> const Type * override { return &boxColliderType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaBoxShape> { return Ref<LuaBoxShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetHalfExtent(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent BoxColliderComponent{{
    {"getHalfExtent", LuaBoxShape::wrap_GetHalfExtent},
    {"getMass", LuaBoxShape::wrap_GetMass},
    {"getInertiaTensor", LuaBoxShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics