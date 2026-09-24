#pragma once

#include "Editor/editor.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <lua.h>
#include <vector>

namespace Engine::Wrap_Editor {

auto wrap_PickObject(lua_State *state) -> int;
auto wrap_SetCamera(lua_State *state) -> int;
auto wrap_SetRelativeMousePosition(lua_State *state) -> int;

auto wrap_StartTranslating(lua_State *state) -> int;
auto wrap_StartRotating(lua_State *state) -> int;
auto wrap_StartScaling(lua_State *state) -> int;
auto wrap_SetTransformAxis(lua_State *state) -> int;
auto wrap_ApplyTransform(lua_State *state) -> int;

static const std::vector<luaL_Reg> EditorLib = {
    {"pickObject", wrap_PickObject},
    {"setCamera", wrap_SetCamera},
    {"setRelativeMousePosition", wrap_SetRelativeMousePosition},
};

static const ::LuaWrap::LuaEnum<TransformAxis> TransformAxisEnum{
    "TransformAxis",
    {
        {"none", TransformAxis::None}, // none
        {"x", TransformAxis::X},       // x | yz plane
        {"y", TransformAxis::Y},       // y | xz plane
        {"z", TransformAxis::Z},       // z | xy plane
        {"yz", TransformAxis::YZ},     // x | yz plane
        {"xz", TransformAxis::XZ},     // y | xz plane
        {"xy", TransformAxis::XY},     // z | xy plane
        {"xyz", TransformAxis::XYZ},   // free
    }};

static const std::vector<lua_CFunction> childrenInitFunctions{};

extern "C" inline auto luaopen_editor(lua_State *state) -> int {
  auto module = ::LuaWrap::LuaModule{
      .Name = "editor",
      .Functions = EditorLib, // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions,
  };

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Engine::Wrap_Editor
