#include "pointLight.hpp"
#include "Modules/object.hpp"
#include "Scene/Lights/light.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<bool, MaxPointLights> UsedPointLightIndices{};

auto PointLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
      Graphics::BufferComponent{
          .name = "Range",
          .format = VK_FORMAT_R32_SFLOAT,
      },
  });

  return format;
}

auto PointLight::Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const -> Error {
  const auto &light = lightEntity.get<Light>();

  auto &format = GetBufferFormat();
  auto offset = light.BufferIndex * format.GetStride();

  if (offset + format.GetStride() > buffer.size()) {
    return Error::Create("Writing light data out of bounds.");
  }

  auto newOffset = light.Write(buffer, offset);

  // NOLINTBEGIN
  auto *floatData = reinterpret_cast<float *>(buffer.data() + newOffset);
  floatData[0] = Range;
  // NOLINTEND

  return {};
}

auto LuaPointLight::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto entity = scene->world.entity(name);
  entity.add<PointLight>();
  entity.add<Light>();
  entity.add<Transform>();
  entity.add<Userdata>();

  auto luaPointLight = Ref<LuaPointLight>::Make(entity);
  ::LuaWrap::PushObject(state, LuaPointLight::GetType(), luaPointLight.get());

  return 1;
}

const ::LuaWrap::LuaClass PointLightClass = {
    .Name = "PointLight",
    .Type = LuaPointLight::GetType(),
    .Methods = {},
};

} // namespace Engine