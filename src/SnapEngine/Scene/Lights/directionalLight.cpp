#include "directionalLight.hpp"
#include "Modules/console.hpp"
#include "Modules/object.hpp"
#include "Scene/Lights/light.hpp"
#include "Scene/frustum.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"
#include <lua.hpp>

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<bool, MaxDirectionalLights> UsedDirectionalLightIndices{};

auto DirectionalLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
  });

  return format;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto DirectionalLight::Write(std::span<uint8_t> buffer,
                             flecs::entity lightEntity) const -> Error {
  const auto &light = lightEntity.get<Light>();
  auto &format = GetBufferFormat();

  auto offset = light.BufferIndex * format.GetStride();

  if (offset + format.GetStride() > buffer.size()) {
    return Error::Create("Writing light data out of bounds.");
  }

  offset = light.Write(buffer, offset);
  const auto &transform = lightEntity.get<Transform>();
  // NOLINTBEGIN
  auto *floatData = reinterpret_cast<float *>(buffer.data() + offset);
  const auto &position = transform.GetPosition();
  const auto &rotation = transform.GetRotation();
  int idx = 0;

  floatData[idx++] = 0.0F;
  floatData[idx++] = 0.0F;
  floatData[idx++] = 0.0F;

  floatData[idx++] = position.x;
  floatData[idx++] = position.y;
  floatData[idx++] = position.z;
  floatData[idx++] = 0.0F; // Padding to align to vec4

  floatData[idx++] = rotation.x;
  floatData[idx++] = rotation.y;
  floatData[idx++] = rotation.z;
  floatData[idx++] = rotation.w;
  // NOLINTEND

  return {};
}

// scene:createDirectionalLight(name, qx, qy, qz, qw, r, g, b, intensity)
// or: scene:createDirectionalLight(name, {qx, qy, qz, qw}, {r, g, b}, intensity)
auto LuaDirectionalLight::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto entity = scene->world.entity();
  entity.add<DirectionalLight>();
  entity.add<Light>();
  entity.add<Transform>();
  entity.add<Userdata>();
  entity.add<Frustum>();
  entity.set<DisplayName>({name});

  bool tableSyntax = false;
  if (lua_istable(state, 3)) {
    tableSyntax = true;
  }

  if (!tableSyntax) {
    auto idx = 3;

    auto x_rot = luaL_optscalar(state, idx++, 0.0F);
    auto y_rot = luaL_optscalar(state, idx++, 0.0F);
    auto z_rot = luaL_optscalar(state, idx++, 0.0F);
    auto w_rot = luaL_optscalar(state, idx++, 1.0F);

    auto red = luaL_optscalar(state, idx++, 1.0F);
    auto green = luaL_optscalar(state, idx++, 1.0F);
    auto blue = luaL_optscalar(state, idx++, 1.0F);
    auto intensity = luaL_optscalar(state, idx++, 1.0F);

    auto &transform = entity.get_mut<Transform>();
    transform.SetRotation(x_rot, y_rot, z_rot, w_rot);
    transform.SetRotation(transform.GetRotation().Normalize());

    auto &light = entity.get_mut<Light>();
    light.SetColor(red, green, blue);
    light.SetIntensity(intensity);
  } else {
    lua_gettable(state, 3);
    if (lua_istable(state, -1)) {
      auto x_rot = luaL_optscalar(state, -1, 0.0F);
      auto y_rot = luaL_optscalar(state, -1, 0.0F);
      auto z_rot = luaL_optscalar(state, -1, 0.0F);
      auto w_rot = luaL_optscalar(state, -1, 1.0F);

      auto &transform = entity.get_mut<Transform>();
      transform.SetRotation(x_rot, y_rot, z_rot, w_rot);
      transform.SetRotation(transform.GetRotation().Normalize());
    }
    lua_pop(state, 1);

    lua_gettable(state, 4);
    if (lua_istable(state, -1)) {
      auto red = luaL_optscalar(state, -1, 1.0F);
      auto green = luaL_optscalar(state, -1, 1.0F);
      auto blue = luaL_optscalar(state, -1, 1.0F);

      auto &light = entity.get_mut<Light>();
      light.SetColor(red, green, blue);
    }
    lua_pop(state, 1);

    auto intensity = luaL_optscalar(state, 5, 1.0F); // NOLINT
    auto &light = entity.get_mut<Light>();
    light.SetIntensity(intensity);
    lua_pop(state, 1);
  }

  auto &light = entity.get_mut<Light>();
  light.Type = LightType::Directional;
  light.BufferIndex = DirectionalLight::GetFreeBufferIndex();
  light.ShadowBufferIndex = 0;

  LUA_ASSERT(light.BufferIndex >= 0 &&
             light.BufferIndex < static_cast<int>(MaxDirectionalLights));

  Renderer::RendererInstance.GetSceneLightBuffers().DirectionalLightCount++;

  auto luaDirectionalLight = Ref<LuaDirectionalLight>::Make(entity);
  ::LuaWrap::PushObject(state, LuaDirectionalLight::GetType(),
                        luaDirectionalLight.get());

  return 1;
}

const ::LuaWrap::LuaClass DirectionalLightClass = {
    .Name = "DirectionalLight",
    .Type = LuaDirectionalLight::GetType(),
    .Methods = {},
};

} // namespace Engine