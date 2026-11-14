#include "geometry.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <imgui.h>

namespace Engine {

auto Geometry::DrawGUI(flecs::entity entity) const -> void {
  if (mesh.isValid()) {
    ImGui::Text("Mesh: %p", mesh.get());
  } else {
    ImGui::Text("Mesh: None");
  }
}

auto LuaGeometry::GetMesh(lua_State *state) -> int {
  auto *entity = LUA_CK_NULL(::LuaWrap::EntityFromLua(state, 1));
  auto geometry = entity->get<Geometry>();

  ::LuaWrap::PushObject(state, Graphics::Mesh::GetType(), geometry.mesh.get());
  return 1;
}

auto LuaGeometry::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto mesh = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Graphics::Mesh>(state, 3));

  auto entity = scene->world.entity(name);
  entity.set<Geometry>(Geometry{.mesh = mesh});
  entity.add<Transform>();

  auto luaGeometry = LuaGeometry::FromEntity(entity);
  ::LuaWrap::PushObject(state, LuaGeometry::GetType(), luaGeometry.get());

  return 1;
}

const ::LuaWrap::LuaComponent GeometryComponent = {{
    {"getMesh", LuaGeometry::GetMesh},
    {"create", LuaGeometry::Create},
}};

} // namespace Engine