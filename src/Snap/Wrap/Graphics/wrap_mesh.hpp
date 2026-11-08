#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/console.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Graphics::Mesh {

auto wrap_SetVertices(lua_State *state) -> int;
auto wrap_SetIndices(lua_State *state) -> int;

auto wrap_SetVertexBuffer(lua_State *state) -> int;
auto wrap_SetIndexBuffer(lua_State *state) -> int;

auto wrap_SetDrawRange(lua_State *state) -> int;
auto wrap_GetDrawRange(lua_State *state) -> int;

auto wrap_GetVertexCount(lua_State *state) -> int;
auto wrap_GetIndexCount(lua_State *state) -> int;

auto wrap_NewMesh(lua_State *state) -> int;
auto wrap_Release(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> MeshLib = {
    {"setVertices", wrap_SetVertices},       {"setIndices", wrap_SetIndices},         {"setVertexBuffer", wrap_SetVertexBuffer},
    {"setIndexBuffer", wrap_SetIndexBuffer}, {"getVertexCount", wrap_GetVertexCount}, {"getIndexCount", wrap_GetIndexCount},
    {"setDrawRange", wrap_SetDrawRange},     {"getDrawRange", wrap_GetDrawRange},

};

extern "C" inline auto luaopen_mesh(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Graphics::Mesh::GetType(),
                           MeshLib); // NOLINT

  return 1;
}

} // namespace Wrap::Graphics::Mesh