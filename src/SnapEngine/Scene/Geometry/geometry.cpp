#include "geometry.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "renderer.hpp"
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

  auto &renderer = Renderer::RendererInstance;

  auto entity = scene->world.entity(name);
  uint32_t transformIndex = LUA_CK_RES(renderer.TransformAllocator.Allocate());
  uint32_t meshIndex = LUA_CK_RES(renderer.MeshAllocator.Allocate());

  entity.set<Geometry>(Geometry{
      .mesh = mesh,
      .transformIndex = static_cast<uint32_t>(transformIndex / renderer.TransformAllocator.itemSize),
      .meshIndex = static_cast<uint32_t>(meshIndex / renderer.MeshAllocator.itemSize),
  });
  entity.add<Transform>();

  auto luaGeometry = LuaGeometry::FromEntity(entity);
  ::LuaWrap::PushObject(state, LuaGeometry::GetType(), luaGeometry.get());

  return 1;
}

const ::LuaWrap::LuaComponent GeometryComponent = {{
    {"getMesh", LuaGeometry::GetMesh},
    {"create", LuaGeometry::Create},
}};

Geometry::~Geometry() {
  auto &renderer = Renderer::RendererInstance;

  if (transformIndex != UINT32_MAX) {
    auto err = Renderer::RendererInstance.TransformAllocator.DeAllocate(transformIndex * renderer.TransformAllocator.itemSize);
  }

  if (meshIndex != UINT32_MAX) {
    auto err = Renderer::RendererInstance.MeshAllocator.DeAllocate(meshIndex * renderer.MeshAllocator.itemSize);
  }
}
} // namespace Engine