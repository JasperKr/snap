#pragma once

#include "Graphics/renderThread.hpp"
#include "Wrap/Graphics/wrap_buffer.hpp"
#include "Wrap/Graphics/wrap_mesh.hpp"
#include "Wrap/Graphics/wrap_rendertarget.hpp"
#include "Wrap/Graphics/wrap_shader.hpp"
#include "Wrap/Graphics/wrap_snapshot.hpp"
#include "Wrap/Graphics/wrap_texture.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Graphics {

auto wrap_Present(lua_State *state) -> int;

// RenderTarget functions
auto wrap_Push(lua_State *state) -> int;
auto wrap_Pop(lua_State *state) -> int;
auto wrap_Reset(lua_State *state) -> int;

// Rendertarget state functions
auto wrap_SetDepthMode(lua_State *state) -> int;
auto wrap_SetCullMode(lua_State *state) -> int;
auto wrap_SetPolygonMode(lua_State *state) -> int;
auto wrap_SetViewport(lua_State *state) -> int;
auto wrap_SetScissor(lua_State *state) -> int;
auto wrap_ClipScissor(lua_State *state) -> int;
auto wrap_SetShader(lua_State *state) -> int;
auto wrap_SetWindingOrder(lua_State *state) -> int;

auto wrap_GetDepthMode(lua_State *state) -> int;
auto wrap_GetCullMode(lua_State *state) -> int;
auto wrap_GetPolygonMode(lua_State *state) -> int;
auto wrap_GetViewport(lua_State *state) -> int;
auto wrap_GetScissor(lua_State *state) -> int;
auto wrap_GetShader(lua_State *state) -> int;
auto wrap_GetRenderTargets(lua_State *state) -> int;
auto wrap_GetWindingOrder(lua_State *state) -> int;

auto wrap_Draw(lua_State *state) -> int;
auto wrap_Dispatch(lua_State *state) -> int;
auto wrap_DispatchIndirect(lua_State *state) -> int;
auto wrap_DrawIndirect(lua_State *state) -> int;

auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;

auto wrap_AcquireCommandBuffer(lua_State *state) -> int;
auto wrap_SubmitCommandBuffer(lua_State *state) -> int;

auto wrap_CopyBuffer(lua_State *state) -> int;
auto wrap_CopyTexture(lua_State *state) -> int;
auto wrap_CopyBufferToTexture(lua_State *state) -> int;
auto wrap_CopyTextureToBuffer(lua_State *state) -> int;

auto wrap_Clear(lua_State *state) -> int;

auto wrap_SetDefaultFilter(lua_State *state) -> int;
auto wrap_GetDefaultFilter(lua_State *state) -> int;
auto wrap_SetDefaultWrapMode(lua_State *state) -> int;
auto wrap_GetDefaultWrapMode(lua_State *state) -> int;

auto wrap_GetStats(lua_State *state) -> int;

auto wrap_PushDebugMarker(lua_State *state) -> int;
auto wrap_PopDebugMarker(lua_State *state) -> int;
auto wrap_PushDebugLabel(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> GraphicsLib = {
    {"present", wrap_Present},
    {"push", wrap_Push},
    {"pop", wrap_Pop},
    {"reset", wrap_Reset},
    {"setDepthMode", wrap_SetDepthMode},
    {"setCullMode", wrap_SetCullMode},
    {"setPolygonMode", wrap_SetPolygonMode},
    {"setViewport", wrap_SetViewport},
    {"setScissor", wrap_SetScissor},
    {"clipScissor", wrap_ClipScissor},
    {"setShader", wrap_SetShader},
    {"setRenderTarget", ::Graphics::RenderState::wrap_SetRenderTargets},
    {"setWindingOrder", wrap_SetWindingOrder},
    {"getDepthMode", wrap_GetDepthMode},
    {"getCullMode", wrap_GetCullMode},
    {"getPolygonMode", wrap_GetPolygonMode},
    {"getViewport", wrap_GetViewport},
    {"getScissor", wrap_GetScissor},
    {"getShader", wrap_GetShader},
    {"getRenderTargets", wrap_GetRenderTargets},
    {"getWindingOrder", wrap_GetWindingOrder},
    {"newTexture", Texture::wrap_NewTexture},
    {"newTextureView", Texture::wrap_NewTextureView},
    {"newMesh", Mesh::wrap_NewMesh},
    {"newShader", Shader::wrap_NewShader},
    {"newBuffer", Buffer::wrap_NewBuffer},
    {"draw", wrap_Draw},
    {"dispatch", wrap_Dispatch},
    {"dispatchIndirect", wrap_DispatchIndirect},
    {"drawIndirect", wrap_DrawIndirect},
    {"getWidth", wrap_GetWidth},
    {"getHeight", wrap_GetHeight},
    {"getDimensions", wrap_GetDimensions},
    {"acquireGraphics", wrap_AcquireCommandBuffer},
    {"submitGraphics", wrap_SubmitCommandBuffer},
    {"readbackBuffer", Buffer::wrap_Readback},
    {"copyBuffer", wrap_CopyBuffer},
    {"copyTexture", wrap_CopyTexture},
    {"copyBufferToTexture", wrap_CopyBufferToTexture},
    {"copyTextureToBuffer", wrap_CopyTextureToBuffer},
    {"clear", wrap_Clear},
    {"setDefaultFilter", wrap_SetDefaultFilter},
    {"getDefaultFilter", wrap_GetDefaultFilter},
    {"setDefaultWrapMode", wrap_SetDefaultWrapMode},
    {"getDefaultWrapMode", wrap_GetDefaultWrapMode},
    {"getStats", wrap_GetStats},
    {"pushDebugMarker", wrap_PushDebugMarker},
    {"popDebugMarker", wrap_PopDebugMarker},
    {"pushDebugLabel", wrap_PushDebugLabel},
};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    Texture::luaopen_texture,   Mesh::luaopen_mesh,
    Shader::luaopen_shader,     Buffer::luaopen_buffer,
    Snapshot::luaopen_snapshot,
};

extern "C" inline auto luaopen_graphics(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "graphics",
      .Functions = GraphicsLib,                       // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT

  };

  LuaWrap::RegisterLuaType(
      state, ::Graphics::Threading::RenderThreadInfo::GetType(), {});

  RegisterLuaModule(state, module);
  return 1;
}

} // namespace Wrap::Graphics