#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <array>
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>
#include <span>
namespace Engine {

constexpr size_t DirectionalLightShadowmapCascadeCount = 4;
constexpr size_t DirectionalLightShadowmapResolution = 2048;
constexpr size_t MaxDirectionalLights = 1;
static const Type directionalLightType = Type("DirectionalLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxDirectionalLights> UsedDirectionalLightIndices;

struct DirectionalLight {
  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxDirectionalLights; ++i) {
      if (!UsedDirectionalLightIndices.at(i)) {
        UsedDirectionalLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaDirectionalLight : LuaWrap::LuaECSObject {
  explicit LuaDirectionalLight(flecs::entity entity) : LuaECSObject(entity) {}

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &directionalLightType; }
  auto GetInstanceType() const -> const Type * override { return &directionalLightType; }
};

extern const ::LuaWrap::LuaClass DirectionalLightClass;

} // namespace Engine