#include "lightProbe.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"
#include <array>
#include <flecs.h>
#include <imgui.h>

namespace Engine::Renderer {

auto LightProbe::Render(const Transform &transform) -> Error {
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  CHECK_ERR(RendererInstance.GetPrefilterManager().PrefilterLightProbe(*ctx, *this, scene, transform));

  return {};
}

auto LightProbe::DrawGUI(flecs::entity entity) -> Error {
  ImGui::Text("Light Probe");
  ImGui::Separator();
  ImGui::DragFloat("Radius", &Radius, 0.1F, 0.0F, 100.0F);            // NOLINT
  ImGui::DragFloat("Inner Radius", &InnerRadius, 0.1F, 0.0F, 100.0F); // NOLINT
  ImGui::Separator();
  InnerRadius = std::min(InnerRadius, Radius);
  ImGui::Text("Environment Map Index: %d", EnvironmentMapIndex);
  if (ImGui::Button("Render Light Probe")) {
    auto *ctx = Graphics::GetCurrentGraphicsContext();
    CHECK_ERR(Render(entity.get<Transform>()));
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

  entity.set(LightProbe{.scene = scene.get()});
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
