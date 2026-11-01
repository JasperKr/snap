#pragma once

#include "Graphics/texture.hpp"
#include "Modules/type.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <lua.h>
namespace Engine {

struct Environment {
  Ref<Graphics::Texture> SkyboxTexture;
};

const Type EnvironmentType = Type("Environment");

struct LuaEnvironment : LuaWrap::LuaECSObject {
  explicit LuaEnvironment(flecs::entity &environment)
      : LuaWrap::LuaECSObject(environment) {}

  static auto GetType() -> const Type * { return &EnvironmentType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override {
    return LuaEnvironment::GetType();
  }

  static auto LoadBinding(lua_State *state) -> int;

  static auto GetSkyboxTexture(lua_State *state) -> int;

  static auto Create(lua_State *state) -> int;
};

auto GetLuaEnvironmentClass() -> const ::LuaWrap::LuaClass &;

} // namespace Engine