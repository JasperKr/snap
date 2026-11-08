#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>
namespace Engine {

struct LevelOfDetail {
  // Threshold is calculated based on the area of the bounding box of the mesh on the screen.
  // Starting at 1.0 and decreasing as the mesh gets smaller on the screen.
  // This threshold determines when to switch to the next level of detail.
  float TransitionThreshold = 0.0F;

  static auto CreateLevelOfDetail(flecs::world &world, const std::string &name, float transitionThreshold) -> flecs::entity {
    auto lod = world.entity(name.c_str());
    lod.set<LevelOfDetail>(LevelOfDetail{.TransitionThreshold = transitionThreshold});

    lod.add<Engine::WorldBounds>();
    lod.add<Engine::LocalBounds>();
    lod.add<Engine::Transform>();

    return lod;
  };

  auto DrawGUI(flecs::entity entity) const -> void;
};

static const Type levelOfDetailType = Type("LevelOfDetail");

struct LuaLevelOfDetail : LuaWrap::LuaECSObject {
  explicit LuaLevelOfDetail(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &levelOfDetailType; }
  auto GetInstanceType() const -> const Type * override { return &levelOfDetailType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaLevelOfDetail> { return Ref<LuaLevelOfDetail>::Make(entity); }

  static auto Create(lua_State *state) -> int;

  static auto GetTransitionThreshold(lua_State *state) -> int;
  static auto SetTransitionThreshold(lua_State *state) -> int;

  static auto AddGeometry(lua_State *state) -> int;
  static auto RemoveGeometry(lua_State *state) -> int;

  static auto GetMeshes(lua_State *state) -> int;
  static auto GetBoundingBox(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaClass LevelOfDetailClass;

} // namespace Engine