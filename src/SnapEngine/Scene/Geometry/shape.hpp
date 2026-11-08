#pragma once

#include "Modules/object.hpp"
#include "Scene/displayName.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <imgui.h>
#include <lua.hpp>
namespace Engine {

struct Shape {
  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type shapeType = Type("Shape");

struct LuaShape : LuaWrap::LuaECSObject {
  explicit LuaShape(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &shapeType; }
  auto GetInstanceType() const -> const Type * override { return &shapeType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaShape> { return Ref<LuaShape>::Make(entity); }

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;
  static auto GetLODs(lua_State *state) -> int;

  static auto Create(lua_State *state) -> int;
};

// extern const ::LuaWrap::LuaClass ShapeClass;
inline auto GetShapeClass() -> ::LuaWrap::LuaClass {
  return {.Name = "Shape",
          .Type = LuaShape::GetType(),
          .Methods =
              {
                  {"getLODs", LuaShape::GetLODs},
              },
          .Components = {
              DisplayNameComponent,
              UserdataComponent,
              TransformComponent,
          }};
}

}; // namespace Engine