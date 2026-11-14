#pragma once

#include "Modules/error.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap_engine.hpp"

namespace Engine {
struct Scene;
}

namespace Engine::Renderer {

struct LightProbe {
  float Radius{};
  float InnerRadius{};
  Scene *scene = nullptr;

  int32_t EnvironmentMapIndex = -1;

  auto Render(const Transform &transform) -> Error;
  auto DrawGUI(flecs::entity entity) -> Error;

  auto WriteToBuffer(std::vector<uint8_t> &buffer, size_t offset, const Transform &transform) const -> void;
};

static const Type LightProbeType = Type("LightProbe");

struct LuaLightProbe : Engine::LuaWrap::LuaECSObject {
  explicit LuaLightProbe(flecs::entity entity) : LuaECSObject(entity) {}

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &LightProbeType; }
  auto GetInstanceType() const -> const Type * override { return &LightProbeType; }

  static auto Render(lua_State *state) -> int;
};

auto GetLuaLightProbeClass() -> ::LuaWrap::LuaClass;

} // namespace Engine::Renderer