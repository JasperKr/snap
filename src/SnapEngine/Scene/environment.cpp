#include "environment.hpp"
#include "Graphics/texture.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Renderer/prefilterManager.hpp"
#include "Scene/displayName.hpp"
#include "Scene/scene.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"

namespace Engine {

auto LuaEnvironment::LoadBinding(lua_State *state) -> int {
  Bindings::LuaBoundStruct<LuaEnvironment> bindings("Environment");
  bindings.DocumentCustomMethod(Bindings::MethodInfo{.name = "getSkyboxTexture", .description = "Gets the skybox texture of this environment."});
  bindings.Register(state);

  return 0;
}

auto LuaEnvironment::GetSkyboxTexture(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaEnvironment>(state, 1));
  const auto *environment = LUA_CK_NULL(obj->entity.try_get<Environment>());

  const auto &skyboxTexture = environment->SkyboxTexture;
  if (skyboxTexture == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  ::LuaWrap::PushObject(state, Graphics::Texture::GetType(), skyboxTexture.get());

  return 1;
}

auto LuaEnvironment::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  const char *name = luaL_checkstring(state, 2);

  auto texture = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Graphics::Texture>(state, 3));

  Environment newEnvironment{};
  newEnvironment.SkyboxTexture = Ref<Graphics::Texture>(texture);

  LUA_CK_ERR(Renderer::RendererInstance.GetPrefilterManager().PrefilterEnvironment(*Graphics::GetCurrentGraphicsContext(), newEnvironment.SkyboxTexture));

  auto environmentEntity = scene->world.entity();
  environmentEntity.set<Environment>(newEnvironment);
  environmentEntity.add<Userdata>();
  environmentEntity.set<DisplayName>({name});

  auto environment = Ref<LuaEnvironment>::Make(environmentEntity);

  ::LuaWrap::PushObject(state, LuaEnvironment::GetType(), environment.get());

  return 1;
}

auto GetLuaEnvironmentClass() -> const ::LuaWrap::LuaClass & {
  static ::LuaWrap::LuaClass lclass = {.Name = "Environment",
                                       .Type = LuaEnvironment::GetType(),
                                       .Components = {
                                           DisplayNameComponent,
                                           UserdataComponent,
                                       }};

  return lclass;
}

} // namespace Engine