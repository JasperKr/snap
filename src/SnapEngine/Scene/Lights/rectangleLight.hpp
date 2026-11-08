#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>

namespace Engine {

constexpr size_t MaxRectangleLights = 32;
static const Type rectangleLightType = Type("RectangleLight");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::array<bool, MaxRectangleLights> UsedRectangleLightIndices;

struct RectangleLight {
  Math::Vec2 Size{1.0F, 1.0F};
  float Range{};

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  auto Write(std::span<uint8_t> buffer, flecs::entity lightEntity) const -> Error;

  // -1 if no free index is available
  static auto GetFreeBufferIndex() -> int32_t {
    for (int32_t i = 0; i < MaxRectangleLights; ++i) {
      if (!UsedRectangleLightIndices.at(i)) {
        UsedRectangleLightIndices.at(i) = true;
        return i;
      }
    }
    return -1;
  }
};

struct LuaRectangleLight : LuaWrap::LuaECSObject {
  explicit LuaRectangleLight(flecs::entity entity) : LuaECSObject(entity) {}

  static auto Create(lua_State *state) -> int;
  static auto GetType() -> const Type * { return &rectangleLightType; }
  auto GetInstanceType() const -> const Type * override { return &rectangleLightType; }
};

extern const ::LuaWrap::LuaClass RectangleLightClass;

} // namespace Engine