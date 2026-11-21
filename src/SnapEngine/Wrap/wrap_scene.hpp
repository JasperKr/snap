#pragma once

#include "Scene/scene.hpp"
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

extern "C" auto luaopen_scene(lua_State *state) -> int;

} // namespace Wrap::Engine