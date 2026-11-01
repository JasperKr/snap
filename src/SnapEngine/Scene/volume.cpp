#include "volume.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/displayName.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "lua.hpp"
#include "renderer.hpp"
#include <imgui.h>
#include <vulkan/vulkan_core.h>

namespace Engine {
using ::LuaWrap::ComponentFromLua;
using ::LuaWrap::ObjectFromLua;

auto Volume::SetTexture(Ref<Graphics::Texture> &texture) -> Error {
  if (texture == nullptr) {
    volumeTexture = nullptr;
    return {};
  }

  ERR_ASSERT_MSG(texture->viewType == VK_IMAGE_VIEW_TYPE_3D,
                 "Texture must be a volume texture");
  ERR_ASSERT_MSG(texture->SupportsSampling(), "Texture must be a sampler");
  ERR_ASSERT_MSG(!texture->IsDepthOrStencilTexture(),
                 "Texture must be of color aspect");

  volumeTexture = texture;

  return {};
}

auto Volume::DrawGUI(flecs::entity entity) -> void {
  ImGui::SeparatorText("Volume Settings");
  ImGui::DragFloat("Density", &density, 0.01F, 0.0F, 1.0F);
  ImGui::DragFloat("Anisotropy", &anisotropy, 0.01F, 0.0F, 1.0F);
}

Volume::~Volume() {
  if (hasTlasIndex) {
    Renderer::RendererInstance.GetVolumeManager().RemoveInstance(tlasIndex);
  }
}

auto LuaVolume::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto texture = ::LuaWrap::ObjectFromLua<Graphics::Texture>(state, 3);

  Volume newVolume{};
  newVolume.volumeTexture = texture;

  auto volumeEntity = scene->world.entity();
  volumeEntity.set<Volume>(newVolume);
  volumeEntity.add<Userdata>();
  volumeEntity.add<Transform>();
  volumeEntity.add<WorldBounds>();
  volumeEntity.set<DisplayName>({name});

  auto environment = Ref<LuaVolume>::Make(volumeEntity);

  ::LuaWrap::PushObject(state, LuaVolume::GetType(), environment.get());

  newVolume.tlasIndex =
      LUA_CK_RES(Renderer::RendererInstance.GetVolumeManager().AddInstance(
          *Graphics::GetCurrentGraphicsContext(), newVolume, {-0.5, -0.5, -0.5},
          {0.5, 0.5, 0.5}));
  newVolume.hasTlasIndex = true;

  return 1;
}

auto LuaVolume::SetAnisotropy(lua_State *state) -> int {
  auto *volume = LUA_CK_RES(ComponentFromLua<Volume>(state, 1));
  volume->anisotropy = luaL_checkscalar(state, 2);

  return 0;
}

auto LuaVolume::SetDensity(lua_State *state) -> int {
  auto *volume = LUA_CK_RES(ComponentFromLua<Volume>(state, 1));
  volume->density = luaL_checkscalar(state, 2);

  return 0;
}

auto LuaVolume::SetTexture(lua_State *state) -> int {
  auto *volume = LUA_CK_RES(ComponentFromLua<Volume>(state, 1));

  // Allowed to be null
  volume->volumeTexture = ObjectFromLua<Graphics::Texture>(state, 2);

  return 0;
}

auto LuaVolume::GetAnisotropy(lua_State *state) -> int {
  auto *volume = LUA_CK_RES(ComponentFromLua<Volume>(state, 1));

  lua_pushnumber(state, volume->anisotropy);

  return 1;
}

auto LuaVolume::GetDensity(lua_State *state) -> int {
  auto *volume = LUA_CK_RES(ComponentFromLua<Volume>(state, 1));

  lua_pushnumber(state, volume->density);

  return 1;
}

auto LuaVolume::GetTexture(lua_State *state) -> int {
  auto *volume = LUA_CK_RES(ComponentFromLua<Volume>(state, 1));

  ::LuaWrap::PushObject(state, Graphics::Texture::GetType(),
                        volume->volumeTexture.get());

  return 1;
}

auto GetLuaVolumeClass() -> const ::LuaWrap::LuaClass & {
  static ::LuaWrap::LuaClass lclass = {
      .Name = "Volume",
      .Type = LuaVolume::GetType(),
      .Methods =
          {
              {"setDensity", LuaVolume::SetDensity},
              {"getDensity", LuaVolume::GetDensity},
              {"setAnisotropy", LuaVolume::SetAnisotropy},
              {"getAnisotropy", LuaVolume::GetAnisotropy},
              {"setTexture", LuaVolume::SetTexture},
              {"getTexture", LuaVolume::GetTexture},
          },
      .Components = {
          TransformComponent,
          DisplayNameComponent,
          UserdataComponent,
      }};

  return lclass;
}

} // namespace Engine