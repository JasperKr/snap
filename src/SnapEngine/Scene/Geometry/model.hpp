#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/displayName.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <string>
namespace Engine {

struct Model {
  auto DrawGUI(flecs::entity entity) const -> void {}
};

static const Type modelType = Type("Model");

struct LuaModel : LuaWrap::LuaECSObject {
  explicit LuaModel(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &modelType; }
  auto GetInstanceType() const -> const Type * override { return &modelType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaModel> { return Ref<LuaModel>::Make(entity); }

  static auto Create(lua_State *state) -> int;
  static auto GetShapes(lua_State *state) -> int;
};

// extern const ::LuaWrap::LuaClass ModelClass;
inline auto GetModelClass() -> ::LuaWrap::LuaClass {
  return {.Name = "Model",
          .Type = LuaModel::GetType(),
          .Methods =
              {
                  {"getShapes", LuaModel::GetShapes},
              },
          .Components = {
              DisplayNameComponent,
              UserdataComponent,
              TransformComponent,
          }};
}

} // namespace Engine