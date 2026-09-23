#include "Wrap/Graphics/wrap_graphics.hpp"

#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/render.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/snapshot.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include "Wrap/Graphics/wrap_color.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap.hpp"
#include <cassert>

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <cstring>
#include <lua.h>
#include <lua.hpp>
#include <string_view>
#include <utility>
#include <vector>

namespace Wrap::Graphics {
auto wrap_Present(lua_State *state) -> int {
  auto &ctx = *::Graphics::GetCurrentGraphicsContext();

  static std::vector<Ref<::Graphics::Threading::RenderThreadInfo>> commands;
  commands.clear();

  if (!lua_isnoneornil(state, 1)) {
    luaL_checktype(state, 1, LUA_TTABLE);

    for (int index = 1; index <= lua_objlen(state, 1); index++) {
      lua_rawgeti(state, 1, index);
      auto renderInfo = LUA_CK_NULL(
          LuaWrap::ObjectFromLua<::Graphics::Threading::RenderThreadInfo>(state,
                                                                          -1));

      commands.emplace_back(renderInfo);
      lua_pop(state, 1);
    }
  }

  LUA_CK_ERR(Present(ctx, commands));

  return 0;
}

// RenderTarget functions
auto wrap_Push(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  LUA_CK_ERR(::Graphics::RenderState::Push(*ctx));
  return 0;
}
auto wrap_Pop(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  LUA_CK_ERR(::Graphics::RenderState::Pop(*ctx));

  return 0;
}
auto wrap_Reset(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  LUA_CK_ERR(::Graphics::RenderState::Reset(*ctx));
  return 0;
}

// Rendertarget state functions

// Options: "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always", writeEnable: bool
auto wrap_SetDepthMode(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  // Enable is set based on the compare op
  const char *compareOpStr = luaL_checkstring(state, 1);
  bool writeEnable = lua_toboolean(state, 2) != 0;
  VkCompareOp compareOp = VK_COMPARE_OP_MAX_ENUM;

  if (strcmp(compareOpStr, "never") == 0) {
    compareOp = VK_COMPARE_OP_NEVER;
  } else if (strcmp(compareOpStr, "less") == 0) {
    compareOp = VK_COMPARE_OP_LESS;
  } else if (strcmp(compareOpStr, "equal") == 0) {
    compareOp = VK_COMPARE_OP_EQUAL;
  } else if (strcmp(compareOpStr, "lequal") == 0) {
    compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  } else if (strcmp(compareOpStr, "greater") == 0) {
    compareOp = VK_COMPARE_OP_GREATER;
  } else if (strcmp(compareOpStr, "notequal") == 0) {
    compareOp = VK_COMPARE_OP_NOT_EQUAL;
  } else if (strcmp(compareOpStr, "gequal") == 0) {
    compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  } else if (strcmp(compareOpStr, "always") == 0) {
    compareOp = VK_COMPARE_OP_ALWAYS;
  } else {
    return luaL_error(state, "Invalid depth compare operation: %s",
                      compareOpStr);
  }

  auto enableRead =
      compareOp != VK_COMPARE_OP_ALWAYS && compareOp != VK_COMPARE_OP_NEVER;

  ::Graphics::RenderState::SetDepthMode(enableRead, writeEnable, compareOp);
  return 0;
}

// Options: none, front, back, always
auto wrap_SetCullMode(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  const char *cullModeStr = luaL_checkstring(state, 1);
  VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;

  if (strcmp(cullModeStr, "none") == 0) {
    cullMode = VK_CULL_MODE_NONE;
  } else if (strcmp(cullModeStr, "front") == 0) {
    cullMode = VK_CULL_MODE_FRONT_BIT;
  } else if (strcmp(cullModeStr, "back") == 0) {
    cullMode = VK_CULL_MODE_BACK_BIT;
  } else if (strcmp(cullModeStr, "always") == 0) {
    cullMode = VK_CULL_MODE_FRONT_AND_BACK;
  } else {
    return luaL_error(state, "Invalid cull mode: %s", cullModeStr);
  }

  ::Graphics::RenderState::SetCullMode(cullMode);
  return 0;
}

// Options: fill, line, point
auto wrap_SetPolygonMode(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  const char *polygonModeStr = luaL_checkstring(state, 1);
  VkPolygonMode polygonMode = VK_POLYGON_MODE_MAX_ENUM;

  if (strcmp(polygonModeStr, "fill") == 0) {
    polygonMode = VK_POLYGON_MODE_FILL;
  } else if (strcmp(polygonModeStr, "line") == 0) {
    polygonMode = VK_POLYGON_MODE_LINE;
  } else if (strcmp(polygonModeStr, "point") == 0) {
    polygonMode = VK_POLYGON_MODE_POINT;
  } else {
    return luaL_error(state, "Invalid polygon mode: %s", polygonModeStr);
  }

  ::Graphics::RenderState::SetPolygonMode(polygonMode);
  return 0;
}

auto wrap_SetViewport(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  if (lua_gettop(state) == 0) {
    ::Graphics::RenderState::SetViewport(nullptr);
    return 0;
  }
  VkViewport viewport{};
  viewport.x = static_cast<float>(luaL_checknumber(state, 1));
  viewport.y = static_cast<float>(luaL_checknumber(state, 2));
  viewport.width = static_cast<float>(luaL_checknumber(state, 3));
  viewport.height = static_cast<float>(luaL_checknumber(state, 4));
  viewport.minDepth =
      static_cast<float>(luaL_optnumber(state, 5, 0.0)); // NOLINT
  viewport.maxDepth =
      static_cast<float>(luaL_optnumber(state, 6, 1.0)); // NOLINT
  ::Graphics::RenderState::SetViewport(&viewport);
  return 0;
}

auto wrap_SetScissor(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (lua_gettop(state) == 0) {
    ::Graphics::RenderState::SetScissor(nullptr);
    return 0;
  }

  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(luaL_checkinteger(state, 1));
  scissor.offset.y = static_cast<int32_t>(luaL_checkinteger(state, 2));
  scissor.extent.width = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  scissor.extent.height = static_cast<uint32_t>(luaL_checkinteger(state, 4));
  ::Graphics::RenderState::SetScissor(&scissor);
  return 0;
}

auto wrap_ClipScissor(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  VkRect2D scissor{};
  scissor.offset.x = static_cast<int32_t>(luaL_checkinteger(state, 1));
  scissor.offset.y = static_cast<int32_t>(luaL_checkinteger(state, 2));
  scissor.extent.width = static_cast<uint32_t>(luaL_checkinteger(state, 3));
  scissor.extent.height = static_cast<uint32_t>(luaL_checkinteger(state, 4));
  ::Graphics::RenderState::ClipScissor(scissor);
  return 0;
}

auto wrap_SetShader(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto shaderHandle = LuaWrap::ObjectFromLua<::Graphics::Shader>(state, 1);
  ::Graphics::RenderState::SetShader(shaderHandle);
  return 0;
}

// Options: ccw, cw
auto wrap_SetWindingOrder(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  VkFrontFace frontFace = VK_FRONT_FACE_MAX_ENUM;

  const char *order = luaL_checkstring(state, 1);
  if (strcmp(order, "cw") == 0) {
    frontFace = VK_FRONT_FACE_CLOCKWISE;
  } else if (strcmp(order, "ccw") == 0) {
    frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  } else {
    return luaL_error(state, "Invalid winding order: %s", order);
  }

  ::Graphics::RenderState::SetWindingOrder(frontFace);
  return 0;
}

// Getters

// Options: "never", "less", "equal", "lequal", "greater", "notequal", "gequal", "always", writeEnable: bool
auto wrap_GetDepthMode(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  bool enableRead = false;
  bool writeEnable = false;
  VkCompareOp compareOp = VK_COMPARE_OP_MAX_ENUM;
  std::tie(enableRead, writeEnable, compareOp) =
      ::Graphics::RenderState::GetDepthMode();

  const char *compareOpStr = "unknown";
  switch (compareOp) {
  case VK_COMPARE_OP_NEVER:
    compareOpStr = "never";
    break;
  case VK_COMPARE_OP_LESS:
    compareOpStr = "less";
    break;
  case VK_COMPARE_OP_EQUAL:
    compareOpStr = "equal";
    break;
  case VK_COMPARE_OP_LESS_OR_EQUAL:
    compareOpStr = "lequal";
    break;
  case VK_COMPARE_OP_GREATER:
    compareOpStr = "greater";
    break;
  case VK_COMPARE_OP_NOT_EQUAL:
    compareOpStr = "notequal";
    break;
  case VK_COMPARE_OP_GREATER_OR_EQUAL:
    compareOpStr = "gequal";
    break;
  case VK_COMPARE_OP_ALWAYS:
    compareOpStr = "always";
    break;
  default:
    break;
  }

  lua_pushstring(state, compareOpStr);
  lua_pushboolean(state, static_cast<int>(writeEnable));
  return 2;
}

// Options: none, front, back, always
auto wrap_GetCullMode(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto cullMode = ::Graphics::RenderState::GetCullMode();

  const char *cullModeStr = "unknown";
  switch (cullMode) {
  case VK_CULL_MODE_NONE:
    cullModeStr = "none";
    break;
  case VK_CULL_MODE_FRONT_BIT:
    cullModeStr = "front";
    break;
  case VK_CULL_MODE_BACK_BIT:
    cullModeStr = "back";
    break;
  case VK_CULL_MODE_FRONT_AND_BACK:
    cullModeStr = "always";
    break;
  default:
    break;
  }

  lua_pushstring(state, cullModeStr);
  return 1;
}

// Options: fill, line, point
auto wrap_GetPolygonMode(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto polygonMode = ::Graphics::RenderState::GetPolygonMode();

  const char *polygonModeStr = "unknown";
  switch (polygonMode) {
  case VK_POLYGON_MODE_FILL:
    polygonModeStr = "fill";
    break;
  case VK_POLYGON_MODE_LINE:
    polygonModeStr = "line";
    break;
  case VK_POLYGON_MODE_POINT:
    polygonModeStr = "point";
    break;
  default:
    break;
  }

  lua_pushstring(state, polygonModeStr);
  return 1;
}

// Options: x, y, width, height, minDepth, maxDepth
auto wrap_GetViewport(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto viewport = ::Graphics::RenderState::GetViewport();

  lua_pushnumber(state, static_cast<lua_Number>(viewport.x));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.y));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.width));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.height));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.minDepth));
  lua_pushnumber(state, static_cast<lua_Number>(viewport.maxDepth));
  return 6; // NOLINT
}

// Options: offsetX, offsetY, extentWidth, extentHeight
auto wrap_GetScissor(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto scissor = ::Graphics::RenderState::GetScissor();

  lua_pushinteger(state, static_cast<lua_Integer>(scissor.offset.x));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.offset.y));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.extent.width));
  lua_pushinteger(state, static_cast<lua_Integer>(scissor.extent.height));
  return 4;
}

// Returns shader handle as integer
auto wrap_GetShader(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto ref = ::Graphics::RenderState::GetUserShader();

  const auto *type = ::Graphics::Shader::GetType();

  LuaWrap::PushObject(state, type, ref.get());

  return 1;
}

auto wrap_GetRenderTargets(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto renderTargets = ::Graphics::RenderState::GetRenderTargets();

  lua_newtable(state);
  for (size_t i = 0; i < renderTargets.size(); ++i) {
    auto &renderTarget = renderTargets[i];
    const auto *type = ::Graphics::RenderState::LuaRenderTarget::GetType();
    auto luaRendertarget =
        Ref<::Graphics::RenderState::LuaRenderTarget>::Make(renderTarget);

    LuaWrap::PushObject(state, type, luaRendertarget.get());
    lua_rawseti(state, -2, static_cast<int>(i + 1));
  }

  return 1;
}
auto wrap_GetWindingOrder(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto frontFace = ::Graphics::RenderState::GetWindingOrder();

  const char *order = "unknown";
  if (frontFace == VK_FRONT_FACE_CLOCKWISE) {
    order = "cw";
  } else if (frontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE) {
    order = "ccw";
  }

  lua_pushstring(state, order);
  return 1;
}

// texture | mesh
auto wrap_Draw(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  Ref<::Graphics::Mesh> mesh;

  if (LuaWrap::IsType<::Graphics::Texture>(state, 1)) {
    auto texture =
        LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));

    auto shader = ::Graphics::RenderState::GetShader();

    LUA_CK_ERR(shader->Send({"MainTexture"}, texture));

    LUA_CK_ERR(::Graphics::Draw(*ctx, *texture, 1));
  } else if (LuaWrap::IsType<::Graphics::Mesh>(state, 1)) {
    mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));
  } else {
    return luaL_error(state, "Invalid argument to draw.");
  }

  auto instanceCount = 1U;

  if (lua_gettop(state) >= 2) {
    instanceCount = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  }

  LUA_CK_ERR(Draw(*ctx, *mesh, instanceCount));

  return 0;
}

auto wrap_Dispatch(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  Math::Uvec3 threadgroups{1, 1, 1};
  threadgroups.x = static_cast<uint32_t>(luaL_checkinteger(state, 1));
  threadgroups.y = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  threadgroups.z = static_cast<uint32_t>(luaL_checkinteger(state, 3));

  LUA_CK_ERR(Dispatch(*ctx, threadgroups));

  return 0;
}

auto wrap_DispatchIndirect(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  auto indirectBuffer =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Buffer>(state, 1));

  auto offset = static_cast<VkDeviceSize>(luaL_optinteger(state, 2, 0));
  LUA_CK_ERR(DispatchIndirect(*ctx, indirectBuffer, offset));

  return 0;
}

auto wrap_DrawIndirect(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));
  auto indirectBuffer =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Buffer>(state, 2));

  auto offset = static_cast<VkDeviceSize>(luaL_optinteger(state, 3, 0));
  auto count = static_cast<uint32_t>(luaL_optinteger(state, 4, 1));
  LUA_CK_ERR(DrawIndirect(*ctx, *mesh, indirectBuffer, offset, count));

  return 0;
}

// Clear the current render target with the given color
// Either: bool (0,0,0,1), bool (depth), bool (stencil)
// Or: Color (attachment 1, vararg), value (depth), value (stencil)
// Or: {[1..]: Color (attachment), depth=value, stencil=value}
// Or: nothing; 0, 0, 0, 1, no depth clear, no stencil clear
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto wrap_Clear(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  ::Graphics::RenderState::ClearInfo clearInfo{};

  if (lua_isboolean(state, 1) != 0) {
    // bool version
    bool clear = lua_toboolean(state, 1) != 0;
    if (clear) {
      clearInfo.colors.emplace_back(0.0F, 0.0F, 0.0F, 1.0F);
    }

    clearInfo.clearDepth =
        lua_isboolean(state, 2) != 0 ? (lua_toboolean(state, 2) != 0) : false;
    clearInfo.clearStencil =
        lua_isboolean(state, 3) != 0 ? (lua_toboolean(state, 3) != 0) : false;
  } else if (lua_isnumber(state, 1) != 0) {
    // Color, depth, stencil version
    auto color = ColorFromLuaState(state, ColorFormat::VarArg, 1);
    clearInfo.colors.emplace_back(color);

    if (lua_isnumber(state, 5) != 0) { // NOLINT
      clearInfo.depthClearValue =
          static_cast<float>(lua_tonumber(state, 5)); // NOLINT
    }

    if (lua_isnumber(state, 6) != 0) { // NOLINT
      clearInfo.stencilClearValue =
          static_cast<int>(lua_tointeger(state, 6)); // NOLINT
    }
  } else if (lua_istable(state, 1) != 0) {
    // Table version
    lua_pushnil(state);
    while (lua_next(state, 1) != 0) {
      // key at -2, value at -1
      if (lua_type(state, -2) == LUA_TNUMBER) {
        // Color entry
        auto color = ColorFromLuaState(state, ColorFormat::List, -1);
        clearInfo.colors.emplace_back(color);
      } else if (lua_type(state, -2) == LUA_TSTRING) {
        const char *key = lua_tostring(state, -2);
        if (strcmp(key, "depth") == 0) {
          clearInfo.depthClearValue =
              static_cast<float>(lua_tonumber(state, -1)); // NOLINT
        } else if (strcmp(key, "stencil") == 0) {
          clearInfo.stencilClearValue =
              static_cast<int>(lua_tointeger(state, -1)); // NOLINT
        }
      }
      lua_pop(state, 1); // pop value, keep key for next iteration
    }
  } else {
    // Only color clear to 0,0,0,1
    clearInfo.clearDepth = false;
    clearInfo.clearStencil = false;
    auto rtCount = ::Graphics::RenderState::GetRenderTargets().size();

    for (size_t i = 0; i < rtCount; i++) {
      clearInfo.colors.emplace_back(0.0F, 0.0F, 0.0F, 1.0F);
    }
  }

  LUA_CK_ERR(::Graphics::RenderState::Clear(*ctx, clearInfo));

  return 0;
}

/*
auto wrap_GetWidth(lua_State *state) -> int;
auto wrap_GetHeight(lua_State *state) -> int;
auto wrap_GetDimensions(lua_State *state) -> int;
*/

auto wrap_GetWidth(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto width = ctx->swapchainInfo.extent.width;
  lua_pushinteger(state, static_cast<lua_Integer>(width));
  return 1;
}

auto wrap_GetHeight(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto height = ctx->swapchainInfo.extent.height;
  lua_pushinteger(state, static_cast<lua_Integer>(height));
  return 1;
}

auto wrap_GetDimensions(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();
  auto extent = ctx->swapchainInfo.extent;

  lua_pushinteger(state, static_cast<lua_Integer>(extent.width));
  lua_pushinteger(state, static_cast<lua_Integer>(extent.height));
  return 2;
}

inline const LuaWrap::LuaEnum<VkQueueFlagBits> queueFlags{
    "DeviceQueue",
    {
        {"Graphics", VK_QUEUE_GRAPHICS_BIT},
        {"Compute", VK_QUEUE_COMPUTE_BIT},
        {"Transfer", VK_QUEUE_TRANSFER_BIT},
    }};

auto wrap_AcquireCommandBuffer(lua_State *state) -> int {
  auto *ctx = LUA_CK_NULL(::Graphics::GetCurrentGraphicsContext());

  ::Graphics::Threading::AcquireInfo info{};
  info.name = luaL_optstring(state, 1, "Unnamed Graphics Commands");
  info.priority = static_cast<int>(luaL_optinteger(state, 2, 0));

  switch (queueFlags.FromLua(state, 3, VK_QUEUE_GRAPHICS_BIT)) {
  case VK_QUEUE_GRAPHICS_BIT:
    info.queueFamily = ctx->graphicsQueueFamily;
    break;
  case VK_QUEUE_COMPUTE_BIT:
    info.queueFamily = ctx->computeQueueFamily;
    break;
  case VK_QUEUE_TRANSFER_BIT:
    info.queueFamily = ctx->transferQueueFamily;
    break;
  default:
    break;
  }

  LUA_ASSERT(info.queueFamily !=
             UINT32_MAX); // Hit if using queue family that isn't present
  LUA_CK_RES(::Graphics::Threading::AcquireCommandBuffer(*ctx, info));

  // Optional boolean arg at idx 4 can be used to indicate that we want to create a performance snapshot for this command buffer
  if (lua_toboolean(state, 4) != 0) {
    ::Graphics::Snapshot::StartSnapshot();
  }

  return 0;
}

auto wrap_SubmitCommandBuffer(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current graphics context.");
  }

  auto submitResult = LUA_CK_RES(::Graphics::Threading::SubmitCommands(*ctx));

  LuaWrap::PushObject(state, ::Graphics::Threading::RenderThreadInfo::GetType(),
                      submitResult.get());

  auto *snapshot = ::Graphics::Snapshot::GetCurrentSnapshot();
  ::Graphics::Snapshot::EndSnapshot();

  if (snapshot != nullptr) {
    // We copy the snapshot data to a new object so the next one isn't overwritten
    auto snapshotCopyResult = snapshot->Copy();

    snapshotCopyResult->events = std::move(snapshot->events);
    snapshotCopyResult->renderStates = std::move(snapshot->renderStates);

    LuaWrap::PushObject(state, ::Graphics::Snapshot::ThreadSnapshot::GetType(),
                        snapshotCopyResult.get());

    return 2;
  }

  return 1;
}

/*
auto wrap_CopyBuffer(lua_State *state) -> int;
auto wrap_CopyTexture(lua_State *state) -> int;
auto wrap_CopyBufferToTexture(lua_State *state) -> int;
auto wrap_CopyTextureToBuffer(lua_State *state) -> int;
*/

auto wrap_CopyBuffer(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto srcBuffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));
  auto dstBuffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 2));

  auto srcIndex = static_cast<size_t>(luaL_checkinteger(state, 3));
  auto dstIndex = static_cast<size_t>(luaL_checkinteger(state, 4));
  auto size = static_cast<size_t>(luaL_checkinteger(state, 5)); // NOLINT

  LUA_CK_ERR(srcBuffer->GetBuffer()->CopyTo(*ctx, *dstBuffer->GetBuffer(),
                                            srcIndex, dstIndex, size));

  return 0;
}

// copyTexture(srcTexture, dstTexture, offsetX=0, offsetY=0, offsetZ=0, extentWidth=src-width, extentHeight=src-height, extentDepth=src-depth, srcBaseMipLevel=0, srcBaseArrayLayer=0, dstBaseMipLevel=0, dstBaseArrayLayer=0, layerCount=1)
auto wrap_CopyTexture(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto srcTexture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));
  auto dstTexture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 2));

  ::Graphics::CopyRegion region{};

  int i = 3; // NOLINT

  region.srcOffset.x = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.srcOffset.y = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.srcOffset.z = static_cast<int32_t>(luaL_optinteger(state, i++, 0));

  region.dstOffset.x = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.dstOffset.y = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.dstOffset.z = static_cast<int32_t>(luaL_optinteger(state, i++, 0));

  region.extent.width = static_cast<uint32_t>(
      luaL_optinteger(state, i++, srcTexture->GetWidth()));
  region.extent.height = static_cast<uint32_t>(
      luaL_optinteger(state, i++, srcTexture->GetHeight()));
  region.extent.depth = static_cast<uint32_t>(
      luaL_optinteger(state, i++, srcTexture->GetDepth()));

  region.srcBaseArrayLayer =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.dstBaseArrayLayer =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.layerCount = static_cast<uint32_t>(luaL_optinteger(state, i++, 1));

  region.srcBaseMipLevel =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.dstBaseMipLevel =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.mipLevelCount = static_cast<uint32_t>(luaL_optinteger(state, i++, 1));

  LUA_CK_ERR(srcTexture->CopyTo(*ctx, *dstTexture, region));

  return 0;
}

// copyBufferToTexture(srcBuffer, dstTexture, bufferOffset=0, bufferRowLength=0, bufferImageHeight=0, imageOffsetX=0, imageOffsetY=0, imageOffsetZ=0, imageExtentWidth=dst-width, imageExtentHeight=dst-height, imageExtentDepth=dst-depth, mipLevel=0, baseArrayLayer=0, layerCount=1)
auto wrap_CopyBufferToTexture(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto srcBuffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));
  auto dstTexture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 2));

  VkBufferImageCopy region{};

  int i = 3; // NOLINT

  region.bufferOffset =
      static_cast<VkDeviceSize>(luaL_optinteger(state, i++, 0));
  region.bufferRowLength =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.bufferImageHeight =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));

  region.imageOffset.x = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.imageOffset.y = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.imageOffset.z = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.imageExtent.width = static_cast<uint32_t>(
      luaL_optinteger(state, i++, dstTexture->GetWidth()));
  region.imageExtent.height = static_cast<uint32_t>(
      luaL_optinteger(state, i++, dstTexture->GetHeight()));
  region.imageExtent.depth = static_cast<uint32_t>(
      luaL_optinteger(state, i++, dstTexture->GetDepth()));

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.imageSubresource.baseArrayLayer =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.imageSubresource.layerCount =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 1));

  LUA_CK_ERR(srcBuffer->GetBuffer()->CopyTo(*ctx, *dstTexture, region));

  return 0;
}

// copyTextureToBuffer(srcTexture, dstBuffer, offsetX=0, offsetY=0, offsetZ=0, extentWidth=src-width, extentHeight=src-height, extentDepth=src-depth, srcBaseMipLevel=0, srcBaseArrayLayer=0, dstOffset=0)
auto wrap_CopyTextureToBuffer(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto srcTexture =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Texture>(state, 1));
  auto dstBuffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 2));

  ::Graphics::ToBufferCopyRegion region{};

  int i = 3; // NOLINT

  region.srcOffset.x = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.srcOffset.y = static_cast<int32_t>(luaL_optinteger(state, i++, 0));
  region.srcOffset.z = static_cast<int32_t>(luaL_optinteger(state, i++, 0));

  region.extent.width = static_cast<uint32_t>(
      luaL_optinteger(state, i++, srcTexture->GetWidth()));
  region.extent.height = static_cast<uint32_t>(
      luaL_optinteger(state, i++, srcTexture->GetHeight()));
  region.extent.depth = static_cast<uint32_t>(
      luaL_optinteger(state, i++, srcTexture->GetDepth()));

  region.srcBaseArrayLayer =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));
  region.layerCount = static_cast<uint32_t>(luaL_optinteger(state, i++, 1));

  region.srcBaseMipLevel =
      static_cast<uint32_t>(luaL_optinteger(state, i++, 0));

  region.dstOffset = static_cast<VkDeviceSize>(luaL_optinteger(state, i++, 0));

  LUA_CK_ERR(srcTexture->CopyTo(*ctx, *dstBuffer->GetBuffer(), region));

  return 0;
}

auto wrap_SetDefaultFilter(lua_State *state) -> int {
  VkFilter minFilter = VK_FILTER_MAX_ENUM;
  VkFilter magFilter = VK_FILTER_MAX_ENUM;

  const char *minFilterStr = luaL_checkstring(state, 1);
  const char *magFilterStr = luaL_optstring(state, 2, minFilterStr);

  if (strcmp(minFilterStr, "nearest") == 0) {
    minFilter = VK_FILTER_NEAREST;
  } else if (strcmp(minFilterStr, "linear") == 0) {
    minFilter = VK_FILTER_LINEAR;
  } else {
    return luaL_error(state, "Invalid min filter: %s", minFilterStr);
  }

  if (strcmp(magFilterStr, "nearest") == 0) {
    magFilter = VK_FILTER_NEAREST;
  } else if (strcmp(magFilterStr, "linear") == 0) {
    magFilter = VK_FILTER_LINEAR;
  } else {
    return luaL_error(state, "Invalid mag filter: %s", magFilterStr);
  }

  auto &config = ::Graphics::Threading::GetGraphicsConfiguration();
  config.defaultSamplerDescription.minFilter = minFilter;
  config.defaultSamplerDescription.magFilter = magFilter;
  config.defaultSamplerDescription.maxAnisotropy = static_cast<float>(
      luaL_optnumber(state, 3, config.defaultSamplerDescription.maxAnisotropy));
  config.defaultSamplerDescription.anisotropyEnable =
      config.defaultSamplerDescription.maxAnisotropy > 1.0F;

  return 0;
}

auto wrap_GetDefaultFilter(lua_State *state) -> int {
  auto &config = ::Graphics::Threading::GetGraphicsConfiguration();

  switch (config.defaultSamplerDescription.minFilter) {
  case VK_FILTER_NEAREST:
    lua_pushstring(state, "nearest");
    break;
  default:
    lua_pushstring(state, "linear");
    break;
  }

  switch (config.defaultSamplerDescription.magFilter) {
  case VK_FILTER_NEAREST:
    lua_pushstring(state, "nearest");
    break;
  default:
    lua_pushstring(state, "linear");
    break;
    break;
  }

  lua_pushnumber(state, static_cast<lua_Number>(
                            config.defaultSamplerDescription.maxAnisotropy));
  return 3;
}

auto inline StringToAddressMode(const char *addressModeStr)
    -> VkSamplerAddressMode {
  if (strcmp(addressModeStr, "repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
  if (strcmp(addressModeStr, "mirrored_repeat") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  }
  if (strcmp(addressModeStr, "clamp_to_edge") == 0) {
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  return VK_SAMPLER_ADDRESS_MODE_REPEAT; // Default
}

auto wrap_SetDefaultWrapMode(lua_State *state) -> int {
  auto &config = ::Graphics::Threading::GetGraphicsConfiguration();

  config.defaultSamplerDescription.addressModeU =
      StringToAddressMode(luaL_checkstring(state, 1));
  config.defaultSamplerDescription.addressModeV =
      StringToAddressMode(luaL_checkstring(state, 2));
  config.defaultSamplerDescription.addressModeW =
      StringToAddressMode(luaL_checkstring(state, 3));

  return 0;
}

auto inline AddressModeToString(VkSamplerAddressMode addressMode) -> const
    char * {
  switch (addressMode) {
  case VK_SAMPLER_ADDRESS_MODE_REPEAT:
    return "repeat";
  case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
    return "mirrored_repeat";
  case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
    return "clamp_to_edge";
  default:
    return "unknown";
  }
}

auto wrap_GetDefaultWrapMode(lua_State *state) -> int {
  auto &config = ::Graphics::Threading::GetGraphicsConfiguration();

  lua_pushstring(state, AddressModeToString(
                            config.defaultSamplerDescription.addressModeU));
  lua_pushstring(state, AddressModeToString(
                            config.defaultSamplerDescription.addressModeV));
  lua_pushstring(state, AddressModeToString(
                            config.defaultSamplerDescription.addressModeW));

  return 3;
}

auto wrap_GetStats(lua_State *state) -> int {
  auto stats = ::Graphics::RenderState::CurrentStats;

  if (lua_istable(state, 1) == 0) {
    lua_newtable(state);
  }

  lua_pushinteger(state, static_cast<lua_Integer>(stats.drawCalls));
  lua_setfield(state, -2, "drawcalls");

  lua_pushinteger(state, static_cast<lua_Integer>(stats.dispatchCalls));
  lua_setfield(state, -2, "dispatches");

  lua_pushinteger(state, static_cast<lua_Integer>(stats.triangleCount));
  lua_setfield(state, -2, "triangles");

  lua_pushinteger(state, static_cast<lua_Integer>(stats.instanceCount));
  lua_setfield(state, -2, "instances");

  lua_pushinteger(state, static_cast<lua_Integer>(stats.contextSwitches));
  lua_setfield(state, -2, "contextswitches");

  lua_pushinteger(state, static_cast<lua_Integer>(
                             ::Graphics::Texture::TotalAllocatedMemory));
  lua_setfield(state, -2, "texturememory");

  lua_pushinteger(state, static_cast<lua_Integer>(
                             ::Graphics::Buffer::TotalAllocatedMemory));
  lua_setfield(state, -2, "buffermemory");

  lua_pushinteger(
      state, static_cast<lua_Integer>(::Graphics::BLAS::TotalAllocatedMemory));
  lua_setfield(state, -2, "blasmemory");

  lua_pushinteger(
      state, static_cast<lua_Integer>(::Graphics::TLAS::TotalAllocatedMemory));
  lua_setfield(state, -2, "tlasmemory");

  return 1;
}

auto wrap_PushDebugMarker(lua_State *state) -> int {
  const auto *name = luaL_checkstring(state, 1);

  if (lua_gettop(state) >= 2) {
    auto color = ColorFromLuaState(state, ColorFormat::VarArg, 2);
    ::Graphics::PushDebugMarker(std::string_view(name), &color);
  } else {
    ::Graphics::PushDebugMarker(std::string_view(name));
  }

  return 0;
}

auto wrap_PopDebugMarker(lua_State *state) -> int {
  ::Graphics::PopDebugMarker();
  return 0;
}

auto wrap_PushDebugLabel(lua_State *state) -> int {
  const auto *name = luaL_checkstring(state, 1);

  if (lua_gettop(state) >= 2) {
    auto color = ColorFromLuaState(state, ColorFormat::VarArg, 2);
    ::Graphics::PushDebugLabel(std::string_view(name), &color);
  } else {
    ::Graphics::PushDebugLabel(std::string_view(name));
  }

  return 0;
}

} // namespace Wrap::Graphics