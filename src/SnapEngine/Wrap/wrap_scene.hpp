#pragma once

#include "Renderer/lightProbe.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/model.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "Scene/camera.hpp"
#include "Scene/environment.hpp"
#include "Scene/scene.hpp"
#include "Scene/volume.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int;
auto wrap_LoadModel(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> SceneLib = {
    {"newScene", wrap_NewScene},
    {"loadModel", wrap_LoadModel},
};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    ::Engine::LuaScene::LoadBinding,
};

extern "C" inline auto luaopen_scene(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "scene",
      .Functions = SceneLib,                          // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT
  };

  RegisterLuaModule(state, module);

  LuaWrap::RegisterLuaType(state, ::Engine::SceneLuaClass);
  LuaWrap::RegisterLuaType(state, ::Engine::GetModelClass());
  LuaWrap::RegisterLuaType(state, ::Engine::GetShapeClass());
  LuaWrap::RegisterLuaType(state, ::Engine::LevelOfDetailClass);
  LuaWrap::RegisterLuaType(state, ::Engine::BoundingBoxClass);
  LuaWrap::RegisterLuaType(state, ::Engine::GeometryClass);

  LuaWrap::RegisterLuaType(state, ::Engine::DirectionalLightClass);
  LuaWrap::RegisterLuaType(state, ::Engine::PointLightClass);
  LuaWrap::RegisterLuaType(state, ::Engine::RectangleLightClass);
  LuaWrap::RegisterLuaType(state, ::Engine::SphereLightClass);
  LuaWrap::RegisterLuaType(state, ::Engine::SpotLightClass);
  LuaWrap::RegisterLuaType(state, ::Engine::GetLuaEnvironmentClass());

  LuaWrap::RegisterLuaType(state, ::Engine::GetLuaCameraClass());
  LuaWrap::RegisterLuaType(state, ::Engine::Renderer::GetLuaLightProbeClass());
  LuaWrap::RegisterLuaType(state, ::Engine::GetLuaVolumeClass());

  return 1;
}

} // namespace Wrap::Engine