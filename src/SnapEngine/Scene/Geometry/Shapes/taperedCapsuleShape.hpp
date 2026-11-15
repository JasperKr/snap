#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h"

namespace Engine::Physics {

struct TaperedCapsuleShape {
  JPH::TaperedCapsuleShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type taperedCapsuleShapeType = Type("TaperedCapsuleShape");

struct LuaTaperedCapsuleShape : LuaWrap::LuaECSObject {
  explicit LuaTaperedCapsuleShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &taperedCapsuleShapeType; }
  auto GetInstanceType() const -> const Type * override { return &taperedCapsuleShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaTaperedCapsuleShape> { return Ref<LuaTaperedCapsuleShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetTopRadius(lua_State *state) -> int;
  static auto wrap_GetBottomRadius(lua_State *state) -> int;
  static auto wrap_GetHalfHeight(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent TaperedCapsuleShapeComponent{{
    {"getTopRadius", LuaTaperedCapsuleShape::wrap_GetTopRadius},
    {"getBottomRadius", LuaTaperedCapsuleShape::wrap_GetBottomRadius},
    {"getHalfHeight", LuaTaperedCapsuleShape::wrap_GetHalfHeight},
    {"getMass", LuaTaperedCapsuleShape::wrap_GetMass},
    {"getInertiaTensor", LuaTaperedCapsuleShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics