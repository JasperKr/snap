#include "wrap_editor.hpp"
#include "Editor/editor.hpp"
#include "Graphics/graphics.hpp"
#include "Scene/camera.hpp"
#include "Wrap/wrap.hpp"
#include <lauxlib.h>

namespace Engine::Wrap_Editor {

auto wrap_PickObject(lua_State *state) -> int {
  Math::Vec2 mousePos =
      Math::Vec2{luaL_checkscalar(state, 1), luaL_checkscalar(state, 2)};

  LUA_CK_ERR(
      Editor::PickEntity(*Graphics::GetCurrentGraphicsContext(), mousePos));

  return 0;
}

auto wrap_SetCamera(lua_State *state) -> int {
  auto camera = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));

  Editor::EditorCamera = camera->entity;

  return 0;
}

auto wrap_SetRelativeMousePosition(lua_State *state) -> int {
  Editor::MoveInfo.CurrentMousePosition = {
      static_cast<float>(luaL_checknumber(state, 1)),
      static_cast<float>(luaL_checknumber(state, 2)),
  };

  return 0;
}

auto wrap_StartTranslating(lua_State *state) -> int {
  Editor::StartTranslating();

  return 0;
}

auto wrap_StartRotating(lua_State *state) -> int {
  Editor::StartRotating();

  return 0;
}

auto wrap_StartScaling(lua_State *state) -> int {
  Editor::StartScaling();

  return 0;
}

auto wrap_SetTransformAxis(lua_State *state) -> int {
  Editor::SetTransformAxis(
      TransformAxisEnum.FromLua(state, 1, TransformAxis::None));

  return 0;
}

auto wrap_ApplyTransform(lua_State *state) -> int {
  Editor::FinalizeTransform();

  return 0;
}

} // namespace Engine::Wrap_Editor