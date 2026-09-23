#include "Wrap/Graphics/wrap_buffer.hpp"

#include "Graphics/snapshot.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/error.hpp"
#include "Wrap/Graphics/wrap_format.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_utils.hpp"
#include <cstdint>
#include <mutex>
#include <public/tracy/Tracy.hpp>
#include <variant>

#include <lua.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/format.hpp"
#include "Modules/bytedata.hpp"

namespace Wrap::Graphics::Buffer {

/*
auto wrap_GetSize(lua_State *state) -> int;
auto wrap_GetElementCount(lua_State *state) -> int;
auto wrap_GetElementStride(lua_State *state) -> int;
auto wrap_Clear(lua_State *state) -> int;
auto wrap_GetFormat(lua_State *state) -> int;
auto wrap_SetData(lua_State *state) -> int;

auto wrap_NewBuffer(lua_State *state) -> int;
*/

auto wrap_GetSize(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetSize()));
  return 1;
}

auto wrap_GetElementCount(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetElementCount()));
  return 1;
}

auto wrap_GetElementStride(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  lua_pushinteger(state, static_cast<lua_Integer>(buffer->GetStride()));
  return 1;
}

// [value = 0], [offset = 0], [size = whole size]
auto wrap_ClearBuffer(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  uint32_t value = 0;
  if (lua_gettop(state) >= 2) {
    value = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  }

  VkDeviceSize offset = 0;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<VkDeviceSize>(luaL_checkinteger(state, 3));
  }

  VkDeviceSize size = VK_WHOLE_SIZE;
  if (lua_gettop(state) >= 4) {
    size = static_cast<VkDeviceSize>(luaL_checkinteger(state, 4));
  }

  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto clearResult = buffer->GetBuffer()->Clear(*ctx, value, offset, size);

  if (Error::IsError(clearResult)) {
    return luaL_error(state, "Failed to clear buffer: %s",
                      clearResult.message.c_str());
  }

  return 0;
}

inline auto BuildBufferFormatTree(lua_State *state,
                                  const ::Graphics::BufferFormat &format)
    -> void {

  // [{}]
  lua_newtable(state);

  int tableIndex = 1;
  for (const auto &component : format.GetComponents()) {
    // [{}, {}]
    lua_newtable(state);
    // name, list of strings
    lua_pushstring(state, "name");
    // [{}, {}, "name"]

    lua_pushstring(state, component.name.c_str());
    // [{}, {}, "name", "componentName"]}]

    // table["name"] = name table
    lua_settable(state, -3); // table["name"] = name

    // offset
    lua_pushstring(state, "offset");
    lua_pushinteger(state, static_cast<lua_Integer>(component.offset));
    lua_settable(state, -3); // table["offset"] = offset

    // format
    lua_pushstring(state, "format");
    if (std::holds_alternative<VkFormat>(component.format)) {
      auto vkFormat = std::get<VkFormat>(component.format);
      lua_pushstring(state, ::Graphics::Format::ToString(vkFormat).data());
    } else if (std::holds_alternative<::Graphics::BufferFormat>(
                   component.format)) {
      auto bufferFormat = std::get<::Graphics::BufferFormat>(component.format);
      BuildBufferFormatTree(state, bufferFormat);
    }
    lua_settable(state, -3); // table["format"] = format

    lua_pushinteger(state, tableIndex++);
    lua_insert(state, -2);   // move index below table
    lua_settable(state, -3); // main table[index] = component table
  }
}

// returns:
// { { name = ..., offset = ..., format = ... } }
auto wrap_GetFormat(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  auto format = buffer->GetFormat();

  BuildBufferFormatTree(state, format);

  return 1;
}

// data: Bytedata | table of numbers, srcOffset: integer, dstOffset: integer, size: integer
// So, stack: [buffer, data, srcOffset?, dstOffset?, size?]
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto wrap_SetData(lua_State *state) -> int {
  ZoneScoped;
  auto *ctx = LUA_CK_NULL(::Graphics::GetCurrentGraphicsContext());

  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  thread_local std::vector<uint8_t> data{};
  snap_defer(data.clear());

  VkDeviceSize offset = 0;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<VkDeviceSize>(luaL_checkinteger(state, 3));
  }

  LUA_ASSERT(offset < buffer->GetSize());
  LUA_ASSERT(offset >= 0);

  VkDeviceSize size = VK_WHOLE_SIZE;
  if (lua_gettop(state) >= 4) {
    size = static_cast<VkDeviceSize>(luaL_checkinteger(state, 4));
  }

#if Enable_Snapshots
  ::Graphics::Snapshot::CaptureEvent(
      ::Graphics::Snapshot::StructuredBufferUploadEvent(
          buffer->GetBuffer()->handle, buffer->GetFormat()));
#endif

  if (lua_istable(state, 2)) {
    // table of numbers

    size_t tableSize = lua_objlen(state, 2);
    data.resize(tableSize * sizeof(float));
    for (int i = 0; i < tableSize; ++i) {
      lua_rawgeti(state, 2, i + 1);
      auto value = luaL_checknumber(state, -1);

      auto result = // NOLINTNEXTLINE; pointer arithmetic
          Wrap::Utils::SetData(value, data.data() + (i * sizeof(float)),
                               buffer->GetFormat().FormatAt(i));

      if (Error::IsError(result)) {
        return luaL_error(state, "Failed to set buffer data: %s",
                          result.message.c_str());
      }

      lua_pop(state, 1);
    }

    auto result = buffer->GetBuffer()->SetData(*ctx, data, offset, size);
    if (Error::IsError(result)) {
      return luaL_error(state, "Failed to set buffer data: %s",
                        result.message.c_str());
    }

  } else {
    auto bytedata =
        LUA_CK_NULL(LuaWrap::ObjectFromLua<Data::ByteData>(state, 2));

    LUA_CK_ERR(buffer->GetBuffer()->SetData(*ctx, bytedata->GetDataSpan(),
                                            offset, size));
  }

  return 0;
}

auto wrap_GetDebugName(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  lua_pushstring(state, buffer->GetBuffer()->debugName.c_str());
  return 1;
}

auto wrap_BufferHasPadding(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  // Todo: allow 140
  auto paddingResult =
      buffer->GetFormat().NeedsPadding(::Graphics::Standard::Std430);

  lua_pushboolean(state, static_cast<int>(paddingResult.needsPadding));
  if (paddingResult.needsPadding) {
    lua_pushstring(state, paddingResult.needsPaddingAt.c_str());
    lua_pushinteger(state,
                    static_cast<lua_Integer>(paddingResult.amountOfPadding));
    return 3;
  }

  return 1;
}

auto wrap_Readback_GetData(lua_State *state) -> int {
  auto readback =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::BufferReadback>(state, 1));

  std::lock_guard<std::mutex> lock(readback->mutex);

  if (!readback->completed) {
    return 0; // Not ready yet, return nil
  }

  LuaWrap::PushObject(state, Data::ByteData::GetType(), readback->data.get());

  return 1;
}

auto wrap_Readback_IsReady(lua_State *state) -> int {
  auto readback =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::BufferReadback>(state, 1));

  std::lock_guard<std::mutex> lock(readback->mutex);

  lua_pushboolean(state, static_cast<int>(readback->completed));
  return 1;
}

auto wrap_Readback_GetError(lua_State *state) -> int {
  auto readback =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::BufferReadback>(state, 1));

  std::lock_guard<std::mutex> lock(readback->mutex);

  if (Error::IsError(readback->error)) {
    lua_pushstring(state, readback->error.message.c_str());
    return 1;
  }

  return 0; // No error, return nil
}

auto wrap_Readback_Wait(lua_State *state) -> int {
  auto readback =
      LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::BufferReadback>(state, 1));

  {
    std::lock_guard<std::mutex> lock(readback->mutex);

    if (readback->completed) {
      return 0;
    }
  }

  {
    std::unique_lock<std::mutex> lock(readback->mutex);
    readback->conditionVar.wait(lock,
                                [&]() -> bool { return readback->completed; });
  }

  return 0;
}

auto wrap_Readback(lua_State *state) -> int {
  auto buffer = LUA_CK_NULL(
      LuaWrap::ObjectFromLua<::Graphics::StructuredBuffer>(state, 1));

  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto readbackResult = buffer->GetBuffer()->Readback(*ctx);

  if (Error::IsError(readbackResult)) {
    return luaL_error(state, "Failed to read back buffer: %s",
                      readbackResult.error().message.c_str());
  }

  auto readbackData = readbackResult.value();

  LuaWrap::PushObject(state, ::Graphics::BufferReadback::GetType(),
                      readbackData.get());

  return 1;
}

// Buffer format: { { name = ..., format = ... }, ... }
inline auto BufferFormatFromLua(lua_State *state, int index)
    -> Result<::Graphics::BufferFormat> {
  std::vector<::Graphics::BufferComponent> components;

  if (lua_type(state, index) == LUA_TSTRING) {
    // Single component format
    return Wrap::Graphics::SimpleFormatFromLua(state, index);
  }

  return Wrap::Graphics::FormatFromLua(state, index);
}

inline auto ConditionalFlag(lua_State *state, VkBufferUsageFlags flag) {
  if (lua_toboolean(state, -1) != 0) {
    lua_pop(state, 1); // pop boolean
    return flag;
  }
  lua_pop(state, 1); // pop boolean

  return static_cast<VkBufferUsageFlags>(0);
}

// usages: "shaderstorage", "uniform", "vertex", "index"
//
// default usages: shaderstorage = false, uniform = false, vertex = false, index = false
//
// format: { { name = ..., format = ... } }
// element count: integer
// settings: { usages, cpupersistent: boolean }
auto wrap_NewBuffer(lua_State *state) -> int {
  auto *ctx = ::Graphics::GetCurrentGraphicsContext();

  if (ctx == nullptr) {
    return luaL_error(state, "No current GraphicsContext set for this thread.");
  }

  auto formatResult = BufferFormatFromLua(state, 1);

  if (Error::IsError(formatResult)) {
    return luaL_error(state, "%s", formatResult.error().message.c_str());
  }

  auto format = formatResult.value();

  auto elementCount = static_cast<size_t>(luaL_checkinteger(state, 2));

  VkMemoryPropertyFlags memoryFlags = 0;
  VkBufferUsageFlags usageFlags = 0;

  if (lua_gettop(state) >= 3) {
    luaL_checktype(state, 3, LUA_TTABLE);

    lua_getfield(state, 3, "shaderstorage");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    lua_getfield(state, 3, "uniform");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    lua_getfield(state, 3, "vertex");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    lua_getfield(state, 3, "index");
    usageFlags |= ConditionalFlag(state, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    lua_getfield(state, 3, "cpupersistent");
    memoryFlags |=
        ConditionalFlag(state, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    lua_getfield(state, 3, "gpuonly");
    if (lua_toboolean(state, -1) != 0) {
      lua_pop(state, 1); // pop boolean

      if ((memoryFlags &
           (static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) != 0U) {
        return luaL_error(state,
                          "Buffer cannot be both cpu persistent and gpu only.");
      }

      memoryFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    } else {
      lua_pop(state, 1); // pop boolean
    }
  }

  if (usageFlags == 0) {
    usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }

  if ((memoryFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0U) {
    usageFlags |=
        VK_BUFFER_USAGE_TRANSFER_DST_BIT; // allow data upload if not gpu only
  }

  if ((usageFlags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0U) {
    // allow readback if not a uniform buffer, since why would you read back a uniform buffer?
    usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }

  ::Graphics::StructuredBufferCreationInfo info;
  info.memoryFlags = memoryFlags;
  info.usageFlags = usageFlags;
  info.debugName = "Structured Buffer";

  auto result =
      ::Graphics::StructuredBuffer::Create(*ctx, format, elementCount, info);

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to create Buffer: %s",
                      result.error().message.c_str());
  }

  auto buffer = result.value();

  LuaWrap::PushObject(state, ::Graphics::StructuredBuffer::GetType(),
                      buffer.get());

  return 1;
}
} // namespace Wrap::Graphics::Buffer