#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>

#include "Jolt/Jolt.h" // IWYU pragma: keep

#include "Jolt/Physics/Collision/Shape/ConvexHullShape.h"

namespace Engine::Physics {

struct ConvexHullShape {
  JPH::ConvexHullShape shape;

  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type convexHullShapeType = Type("ConvexHullShape");

struct LuaConvexHullShape : LuaWrap::LuaECSObject {
  explicit LuaConvexHullShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &convexHullShapeType; }
  auto GetInstanceType() const -> const Type * override { return &convexHullShapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaConvexHullShape> { return Ref<LuaConvexHullShape>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto wrap_GetPlanes(lua_State *state) -> int;
  static auto wrap_GetPoint(lua_State *state) -> int;
  static auto wrap_GetNumFaces(lua_State *state) -> int;
  static auto wrap_GetNumVerticesInFace(lua_State *state) -> int;
  static auto wrap_GetFaceVertices(lua_State *state) -> int;
  static auto wrap_GetMass(lua_State *state) -> int;
  static auto wrap_GetInertiaTensor(lua_State *state) -> int;
};

const ::LuaWrap::LuaComponent ConvexHullShapeComponent{{
    {"getPlanes", LuaConvexHullShape::wrap_GetPlanes},
    {"getPoint", LuaConvexHullShape::wrap_GetPoint},
    {"getNumFaces", LuaConvexHullShape::wrap_GetNumFaces},
    {"getNumVerticesInFace", LuaConvexHullShape::wrap_GetNumVerticesInFace},
    {"getFaceVertices", LuaConvexHullShape::wrap_GetFaceVertices},
    {"getMass", LuaConvexHullShape::wrap_GetMass},
    {"getInertiaTensor", LuaConvexHullShape::wrap_GetInertiaTensor},
}};

} // namespace Engine::Physics