#pragma once

#include "lua.hpp"
namespace Graphics::RenderState {
auto wrap_SetRenderTargets(lua_State *state) -> int;
}