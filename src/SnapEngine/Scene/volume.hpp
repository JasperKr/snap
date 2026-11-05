#pragma once

#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <cstdint>
#include <lua.h>

namespace Engine {

struct Volume { // NOLINT

  Volume(const Volume &) = delete;
  Volume(Volume &&) = delete;
  auto operator=(const Volume &) -> Volume & = delete;
  auto operator=(Volume &&) -> Volume & = delete;
  Volume() = default;

  // Allowed to be null
  Ref<Graphics::Texture> volumeTexture;

  float density = 0.1;
  float anisotropy = 0.4;
  uint32_t tlasIndex{}; // Index into scene volume TLAS
  bool hasTlasIndex = false;

  auto SetTexture(Ref<Graphics::Texture> &texture) -> Error;
  auto DrawGUI(flecs::entity entity) -> void;

  ~Volume();
};

const static Type VolumeType = Type("Volume");

struct LuaVolume : LuaWrap::LuaECSObject {
  static auto GetType() -> const Type * { return &VolumeType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override {
    return LuaVolume::GetType();
  }

  explicit LuaVolume(flecs::entity &volume) : LuaWrap::LuaECSObject(volume) {}

  static auto Create(lua_State *state) -> int;

  static auto SetDensity(lua_State *state) -> int;
  static auto GetDensity(lua_State *state) -> int;
  static auto SetAnisotropy(lua_State *state) -> int;
  static auto GetAnisotropy(lua_State *state) -> int;
  static auto SetTexture(lua_State *state) -> int;
  static auto GetTexture(lua_State *state) -> int;
};

auto GetLuaVolumeClass() -> const ::LuaWrap::LuaClass &;

} // namespace Engine