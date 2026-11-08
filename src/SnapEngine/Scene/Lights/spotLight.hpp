#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>
#include <span>
namespace Engine {

constexpr size_t MaxSpotLights = 32;
static const Type spotLightType = Type("SpotLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxSpotLights> UsedSpotLightIndices;

struct SpotLight {
  float Range{};
  float InnerConeAngle{};
  float OuterConeAngle{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxSpotLights; ++i) {
      if (!UsedSpotLightIndices.at(i)) {
        UsedSpotLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaSpotLight : LuaWrap::LuaECSObject {
  explicit LuaSpotLight(flecs::entity entity) : LuaECSObject(entity) {}

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &spotLightType; }
  auto GetInstanceType() const -> const Type * override { return &spotLightType; }
};

extern const ::LuaWrap::LuaClass SpotLightClass;

} // namespace Engine