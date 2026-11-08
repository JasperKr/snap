#include "rectangleLight.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/Lights/light.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<bool, MaxRectangleLights> UsedRectangleLightIndices{};

auto RectangleLight::GetBufferFormat() -> Graphics::BufferFormat & {
  static auto format = Graphics::BufferFormat({
      Graphics::BufferComponent{
          .name = "Base",
          .format = Light::GetBufferFormat(),
      },
      Graphics::BufferComponent{
          .name = "Size",
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      Graphics::BufferComponent{
          .name = "Range",
          .format = VK_FORMAT_R32_SFLOAT,
      },
  });

  return format;
}

auto RectangleLight::Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const -> Error {
  const auto &light = lightEntity.get<Light>();

  auto &format = GetBufferFormat();
  auto offset = light.BufferIndex * format.GetStride();

  ERR_ASSERT_MSG(offset + format.GetStride() <= buffer.size(), "Writing light data out of bounds.");

  auto newOffset = light.Write(buffer, offset);

  // NOLINTBEGIN
  auto *floatData = reinterpret_cast<float *>(buffer.data() + newOffset);
  floatData[0] = Size.x;
  floatData[1] = Size.y;
  floatData[2] = Range;
  // NOLINTEND

  return {};
}

auto LuaRectangleLight::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto entity = scene->world.entity(name);
  entity.add<RectangleLight>();
  entity.add<Light>();
  entity.add<Transform>();
  entity.add<Userdata>();

  auto luaRectangleLight = Ref<LuaRectangleLight>::Make(entity);
  ::LuaWrap::PushObject(state, LuaRectangleLight::GetType(), luaRectangleLight.get());

  return 1;
}

const ::LuaWrap::LuaClass RectangleLightClass = {
    .Name = "RectangleLight",
    .Type = LuaRectangleLight::GetType(),
    .Methods = {},
};

} // namespace Engine