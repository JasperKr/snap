#include "sphereLight.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "light.hpp"
#include <flecs.h>
#include <span>

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<bool, MaxSphereLights> UsedSphereLightIndices{};

auto SphereLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
      Graphics::BufferComponent{
          .name = "Range",
          .format = VK_FORMAT_R32_SFLOAT,
      },
      Graphics::BufferComponent{
          .name = "Radius",
          .format = VK_FORMAT_R32_SFLOAT,
      },
  });

  return format;
}

auto SphereLight::Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const -> Error {
  const auto &light = lightEntity.get<Light>();

  auto &format = GetBufferFormat();
  auto offset = light.BufferIndex * format.GetStride();

  ERR_ASSERT_MSG(offset + format.GetStride() <= buffer.size(), "Writing light data out of bounds.");

  auto newOffset = light.Write(buffer, offset);

  // NOLINTBEGIN
  auto *floatData = reinterpret_cast<float *>(buffer.data() + newOffset);
  floatData[0] = Range;
  floatData[1] = Radius;
  // NOLINTEND

  return {};
}

auto LuaSphereLight::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto entity = scene->world.entity(name);
  entity.add<SphereLight>();
  entity.add<Light>();
  entity.add<Transform>();
  entity.add<Userdata>();

  auto luaSphereLight = Ref<LuaSphereLight>::Make(entity);
  ::LuaWrap::PushObject(state, LuaSphereLight::GetType(), luaSphereLight.get());

  return 1;
}

const ::LuaWrap::LuaClass SphereLightClass = {
    .Name = "SphereLight",
    .Type = LuaSphereLight::GetType(),
    .Methods = {},
};

} // namespace Engine
