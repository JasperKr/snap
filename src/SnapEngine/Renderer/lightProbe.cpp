#include "lightProbe.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Renderer/prefilterManager.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "defer.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <array>
#include <flecs.h>
#include <imgui.h>
#include <lauxlib.h>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

auto LightProbe::Render(const Transform &transform) -> Error {
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  CHECK_ERR(RendererInstance.GetPrefilterManager().PrefilterLightProbe(*ctx, *this, *scene, transform));

  return {};
}

auto LightProbe::DrawGUI(flecs::entity entity) -> Error {
  auto &context = *Graphics::GetCurrentGraphicsContext();
  auto &manager = RendererInstance.GetPrefilterManager();

  ImGui::Text("Light Probe");
  ImGui::Separator();
  ImGui::DragFloat("Radius", &Radius, 0.1F, 0.0F, 100.0F);            // NOLINT
  ImGui::DragFloat("Inner Radius", &InnerRadius, 0.1F, 0.0F, 100.0F); // NOLINT
  ImGui::Separator();
  InnerRadius = std::min(InnerRadius, Radius);
  ImGui::Text("Environment Map Index: %d", EnvironmentMapIndex);
  if (ImGui::Button("Render Light Probe")) {
    CHECK_ERR(Render(entity.get<Transform>()));
  }

  if (ImGui::CollapsingHeader("Maps") && EnvironmentMapIndex != -1) {
    static int Mip = 0;

    auto irradiance = CHECK_RES(Graphics::Texture::Create(context, manager.GetIrradianceMaps().get(), VK_IMAGE_VIEW_TYPE_2D,
                                                          VkImageSubresourceRange{
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = static_cast<uint32_t>(EnvironmentMapIndex),
                                                              .layerCount = 1,
                                                          }));

    auto size = 200;

    ImGui::Image(reinterpret_cast<ImTextureID>(irradiance.get()), ImVec2(size, size)); //NOLINT

    int mipmapCount = static_cast<int>(manager.GetRadianceMaps()->GetMipmapCount());
    ImGui::SliderInt("Mip", &Mip, 0, mipmapCount - 1);

    Mip = std::clamp(Mip, 0, mipmapCount - 1);

    auto radiance = CHECK_RES(Graphics::Texture::Create(context, manager.GetRadianceMaps().get(), VK_IMAGE_VIEW_TYPE_2D,
                                                        VkImageSubresourceRange{
                                                            .baseMipLevel = static_cast<uint32_t>(Mip),
                                                            .levelCount = 1,
                                                            .baseArrayLayer = static_cast<uint32_t>(EnvironmentMapIndex),
                                                            .layerCount = 1,
                                                        }));

    ImGui::Image(reinterpret_cast<ImTextureID>(radiance.get()), ImVec2(size, size)); //NOLINT

    irradiance->MarkUse();
    radiance->MarkUse();

    irradiance->retain();
    radiance->retain();

    DeferredManager::Instance.Register({
        .callOnShutdown = true,
        .frames = 1UL,
        .action = [irradiance, radiance]() -> void {
          irradiance->release();
          radiance->release();
        },
    });
  }

  return {};
}

/*
struct LightProbe {
  float3 position;
  int index;

  float radius;
  float innerRadius;
};
*/

auto LightProbe::WriteToBuffer(std::vector<uint8_t> &buffer, size_t offset, const Transform &transform) const -> void {
  // NOLINTBEGIN
  auto *ptr = buffer.data() + offset;

  auto translation = transform.GetWorldMatrix().GetTranslation();
  std::array<float, 3> position = {translation.x, translation.y, translation.z};

  std::memcpy(ptr, position.data(), sizeof(float) * 3);
  ptr += sizeof(float) * 3;

  std::memcpy(ptr, &EnvironmentMapIndex, sizeof(int32_t));
  ptr += sizeof(int32_t);

  std::memcpy(ptr, &Radius, sizeof(float));
  ptr += sizeof(float);

  std::memcpy(ptr, &InnerRadius, sizeof(float));

  // NOLINTEND
}

auto LuaLightProbe::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  auto entity = scene->world.entity();

  double radius = luaL_optnumber(state, 3, 10.0);
  double innerRadius = luaL_optnumber(state, 4, radius / 2.0);

  entity.set(LightProbe{.Radius = static_cast<float>(radius), .InnerRadius = static_cast<float>(innerRadius), .scene = scene.get()});
  entity.add<Transform>();
  entity.set<DisplayName>({luaL_optstring(state, 2, "LightProbe")});
  entity.add<Userdata>();

  auto luaProbe = Ref<LuaLightProbe>::Make(entity);

  ::LuaWrap::PushObject(state, LuaLightProbe::GetType(), luaProbe.get());
  return 1;
}

auto LuaLightProbe::Render(lua_State *state) -> int {
  ZoneScoped;

  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaLightProbe>(state, 1));
  auto *lightProbe = LUA_CK_NULL(obj->entity.try_get_mut<LightProbe>());
  const auto *transform = LUA_CK_NULL(obj->entity.try_get<Transform>());

  LUA_CK_ERR(lightProbe->Render(*transform));

  return 0;
}

auto GetLuaLightProbeClass() -> ::LuaWrap::LuaClass {
  const ::LuaWrap::LuaClass LuaCameraClass = {.Name = "LightProbe",
                                              .Type = LuaLightProbe::GetType(),
                                              .Methods =
                                                  {
                                                      {"render", LuaLightProbe::Render},
                                                  },
                                              .Components = {
                                                  TransformComponent,
                                                  DisplayNameComponent,
                                                  UserdataComponent,
                                              }};

  return LuaCameraClass;
}

} // namespace Engine::Renderer
