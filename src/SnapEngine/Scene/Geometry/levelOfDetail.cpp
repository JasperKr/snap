#include "levelOfDetail.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/scene.hpp"
#include "Wrap/wrap.hpp"
#include <imgui.h>
#include <lua.hpp>

namespace Engine {

auto LevelOfDetail::DrawGUI(flecs::entity entity) const -> void { ImGui::Text("Transition Threshold: %.2f", TransitionThreshold); }

auto LuaLevelOfDetail::GetTransitionThreshold(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = entity->get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  lua_pushnumber(state, levelOfDetail->TransitionThreshold);
  return 1;
}
auto LuaLevelOfDetail::SetTransitionThreshold(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = entity->get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  levelOfDetail->TransitionThreshold = static_cast<float>(luaL_checknumber(state, 2));
  return 0;
}

auto LuaLevelOfDetail::GetMeshes(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = entity->get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  if (lua_gettop(state) < 2) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  int index = 1;
  levelOfDetail.entity().children<Graphics::Mesh>([&](const flecs::entity &child) -> void {
    lua_pushinteger(state, index++);
    auto mesh = child.get_ref<Graphics::Mesh>();
    ::LuaWrap::PushObject(state, Graphics::Mesh::GetType(), mesh.get());
    lua_settable(state, -3);
  });

  return 1;
}

auto LuaLevelOfDetail::AddGeometry(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = entity->get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  auto geometry = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaGeometry>(state, 2));
  if (geometry == nullptr) {
    return luaL_error(state, "Expected a Mesh object");
  }

  geometry->entity.child_of(levelOfDetail.entity());

  return 0;
}

auto LuaLevelOfDetail::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  if (scene == nullptr) {
    return luaL_error(state, "Expected a World object");
  }

  auto transitionThreshold = static_cast<float>(luaL_optnumber(state, 3, 0.0F));

  auto lodEntity = LevelOfDetail::CreateLevelOfDetail(scene->world, name, transitionThreshold);
  auto entity = LuaLevelOfDetail::FromEntity(lodEntity);
  ::LuaWrap::PushObject(state, LuaLevelOfDetail::GetType(), entity.get());
  return 1;
}

auto LuaLevelOfDetail::RemoveGeometry(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = entity->get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  auto geometry = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaGeometry>(state, 2));

  levelOfDetail.entity().remove<Ref<Graphics::Mesh>>(geometry->entity);

  return 0;
}

auto LuaLevelOfDetail::GetBoundingBox(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Expected a LevelOfDetail object");
  }

  auto levelOfDetail = entity->get_ref<LevelOfDetail>();
  if (levelOfDetail.get() == nullptr) {
    return luaL_error(state, "LevelOfDetail component not found");
  }

  if (levelOfDetail.entity().has<BoundingBox>()) {
    auto boundingBoxRef = levelOfDetail.entity().get_ref<BoundingBox>();
    auto *boundingBox = boundingBoxRef.get();

    lua_pushnumber(state, boundingBox->Min.x);
    lua_pushnumber(state, boundingBox->Min.y);
    lua_pushnumber(state, boundingBox->Min.z);

    lua_pushnumber(state, boundingBox->Max.x);
    lua_pushnumber(state, boundingBox->Max.y);
    lua_pushnumber(state, boundingBox->Max.z);

    return 6; // NOLINT
  }

  return 0; // No bounding box found, return 0 values
}

const ::LuaWrap::LuaClass LevelOfDetailClass = {
    .Name = "LevelOfDetail",
    .Type = LuaLevelOfDetail::GetType(),
    .Methods =
        {
            {"getTransitionThreshold", LuaLevelOfDetail::GetTransitionThreshold},
            {"setTransitionThreshold", LuaLevelOfDetail::SetTransitionThreshold},
            {"getMeshes", LuaLevelOfDetail::GetMeshes},
            {"addGeometry", LuaLevelOfDetail::AddGeometry},
            {"removeGeometry", LuaLevelOfDetail::RemoveGeometry},
            {"getBoundingBox", LuaLevelOfDetail::GetBoundingBox},
        },
};

} // namespace Engine