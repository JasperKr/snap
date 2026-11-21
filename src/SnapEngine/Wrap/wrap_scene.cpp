#include "wrap_scene.hpp"
#include "Graphics/graphics.hpp"
#include "Modules/object.hpp"
#include "Renderer/lightProbe.hpp"
#include "Scene/Geometry/boundingBox.hpp"
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
#include "gltfLoader.hpp"

namespace Wrap::Engine {

auto wrap_NewScene(lua_State *state) -> int {
  const char *name = luaL_checkstring(state, 1);

  auto scene = Ref<::Engine::Scene>::Make(name);

  LuaWrap::PushObject(state, ::Engine::Scene::GetType(), scene.get());
  return 1;
}

auto wrap_LoadModel(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Engine::Scene>(state, 1));

  const char *path = luaL_checkstring(state, 2);

  auto *ctx = Graphics::GetCurrentGraphicsContext();

  auto loadResult = glTF::LoadGltfModel(*ctx, path, &scene->world);
  if (Error::IsError(loadResult)) {
    luaL_error(state, loadResult.message.c_str());
  }

  return 0;
}

auto luaopen_scene(lua_State *state) -> int {
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