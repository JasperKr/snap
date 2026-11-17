#pragma once

#include "Jolt/Jolt.h" // IWYU pragma: keep
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Physics/Body/Body.h"

namespace Engine::Physics {

struct Rigidbody {
  JPH::Body *body;

  auto DrawGUI(flecs::entity entity) const -> void;
};

static const Type rigidbodyType = Type("Rigidbody");

struct LuaRigidbody : LuaWrap::LuaECSObject {
  explicit LuaRigidbody(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &rigidbodyType; }
  auto GetInstanceType() const -> const Type * override { return &rigidbodyType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaRigidbody> { return Ref<LuaRigidbody>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_IsActive(lua_State *state) -> int;
  static auto wrap_IsStatic(lua_State *state) -> int;
  static auto wrap_IsKinematic(lua_State *state) -> int;
  static auto wrap_IsDynamic(lua_State *state) -> int;
  static auto wrap_CanBeNonStatic(lua_State *state) -> int;
  static auto wrap_IsSensor(lua_State *state) -> int;
  static auto wrap_UseManifoldReduction(lua_State *state) -> int;
  static auto wrap_GetUseManifoldReduction(lua_State *state) -> int;
  static auto wrap_SetMotionType(lua_State *state) -> int;
  static auto wrap_GetMotionType(lua_State *state) -> int;
  static auto wrap_SetAllowSleeping(lua_State *state) -> int;
  static auto wrap_GetAllowSleeping(lua_State *state) -> int;
  static auto wrap_ResetSleepTimer(lua_State *state) -> int;
  static auto wrap_SetFriction(lua_State *state) -> int;
  static auto wrap_GetFriction(lua_State *state) -> int;
  static auto wrap_SetRestitution(lua_State *state) -> int;
  static auto wrap_GetRestitution(lua_State *state) -> int;
  static auto wrap_SetLinearVelocity(lua_State *state) -> int;
  static auto wrap_GetLinearVelocity(lua_State *state) -> int;
  static auto wrap_SetAngularVelocity(lua_State *state) -> int;
  static auto wrap_GetAngularVelocity(lua_State *state) -> int;
  static auto wrap_GetPointVelocity(lua_State *state) -> int;
  static auto wrap_AddForce(lua_State *state) -> int;
  static auto wrap_AddImpulse(lua_State *state) -> int;
  static auto wrap_AddAngularImpulse(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent RigidbodyComponent{{

}};

} // namespace Engine::Physics