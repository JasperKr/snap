#include "Wrap/Graphics/wrap_mesh.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <lua.hpp>
#include <unordered_map>
#include <vector>

#include <cmath>
#include <vulkan/vulkan_core.h>

namespace Wrap::Graphics::Mesh {

// Bytedata, [offset], [range]
auto wrap_SetVertices(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));
  auto data = LUA_CK_NULL(LuaWrap::ObjectFromLua<Data::ByteData>(state, 2));

  auto offset = 0L;
  if (lua_gettop(state) >= 3) {
    offset = static_cast<int64_t>(luaL_checkinteger(state, 3));
  }

  LUA_ASSERT(offset >= 0);
  LUA_ASSERT(offset < data->GetSize());

  auto count = data->GetSize() - offset;
  if (lua_gettop(state) >= 4) {
    count = static_cast<int64_t>(luaL_checkinteger(state, 4));

    LUA_ASSERT(count >= 0);
  }

  LUA_ASSERT(offset + count <= data->GetSize());

  std::span<uint8_t> vertexData = // NOLINTNEXTLINE, pointer arithmetic
      std::span<uint8_t>(data->GetData() + offset, static_cast<size_t>(count));

  auto *ctx = LUA_CK_NULL(::Graphics::GetCurrentGraphicsContext());
  LUA_CK_ERR(
      mesh->SetVertices(*ctx, 0, vertexData, static_cast<uint64_t>(offset)));

  return 0;
}

// data: bytedata, offset: int, precision: [uint32_t] | uint16_t
auto wrap_SetIndices(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));
  auto data = LUA_CK_NULL(LuaWrap::ObjectFromLua<Data::ByteData>(state, 2));

  auto format = VK_INDEX_TYPE_UINT32;

  if (lua_gettop(state) >= 4) {
    const auto *luaStrFormat = luaL_checkstring(state, 4);

    if (std::strcmp(luaStrFormat, "uint32_t") == 0) {
      format = VK_INDEX_TYPE_UINT32;
    } else if (std::strcmp(luaStrFormat, "uint16_t") == 0) {
      format = VK_INDEX_TYPE_UINT16;
    } else if (std::strcmp(luaStrFormat, "uint8_t") == 0) {
      format = VK_INDEX_TYPE_UINT8;
    } else {
      return luaL_error(state, "Unknown index data format: %s", luaStrFormat);
    }
  }

  auto count = data->GetSize() / ::Graphics::GetIndexFormatSize(format);
  if (lua_gettop(state) >= 3) {
    count = static_cast<int64_t>(luaL_checkinteger(state, 3));

    LUA_ASSERT(count >= 0);
  }

  LUA_ASSERT(count <= data->GetSize() / ::Graphics::GetIndexFormatSize(format));

  std::span<uint8_t> indexData = std::span<
      uint8_t>( // NOLINTNEXTLINE, pointer arithmetic and reinterpret cast
      reinterpret_cast<uint8_t *>(data->GetData()),
      (count * ::Graphics::GetIndexFormatSize(format)));

  auto *ctx = LUA_CK_NULL(::Graphics::GetCurrentGraphicsContext());
  LUA_CK_ERR(mesh->SetIndices(*ctx, indexData, format));

  return 0;
}

auto wrap_SetVertexBuffer(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));

  auto buffer = LuaWrap::ObjectFromLua<::Graphics::Buffer>(state, 2);

  mesh->SetVertexBuffer(Ref<::Graphics::Buffer>(buffer));

  return 0;
}
auto wrap_SetIndexBuffer(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));

  auto buffer = LuaWrap::ObjectFromLua<::Graphics::Buffer>(state, 2);

  auto format = VK_INDEX_TYPE_UINT32;

  if (lua_gettop(state) >= 3) {
    const auto *luaStrFormat = luaL_checkstring(state, 3);

    if (std::strcmp(luaStrFormat, "uint32_t") == 0) {
      format = VK_INDEX_TYPE_UINT32;
    } else if (std::strcmp(luaStrFormat, "uint16_t") == 0) {
      format = VK_INDEX_TYPE_UINT16;
    } else if (std::strcmp(luaStrFormat, "uint8_t") == 0) {
      format = VK_INDEX_TYPE_UINT8;
    } else {
      return luaL_error(state, "Unknown index data format: %s", luaStrFormat);
    }
  }

  LUA_CK_ERR(mesh->SetIndexBuffer(Ref<::Graphics::Buffer>(buffer), format));

  return 0;
}

auto wrap_SetDrawRange(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));

  auto offset = static_cast<uint32_t>(luaL_checkinteger(state, 2));
  auto count = static_cast<uint32_t>(luaL_checkinteger(state, 3));

  mesh->SetDrawRange(
      ::Graphics::MeshDrawRange{.Offset = offset, .Count = count});

  return 0;
}
auto wrap_GetDrawRange(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));

  auto range = mesh->GetDrawRange();

  lua_pushinteger(state, static_cast<lua_Integer>(range.Offset));
  lua_pushinteger(state, static_cast<lua_Integer>(range.Count));

  return 2;
}

// Vertex formats are laid out as:
/*
{
  { name = "name", format = "float", location = 0 },
  { name = "name", format = "uint32", location = 1 },
}
*/
inline auto VertexFormatFromLua(lua_State *state, int index)
    -> Result<std::vector<::Graphics::VertexComponent>> {
  luaL_checktype(state, index, LUA_TTABLE);

  std::vector<::Graphics::VertexComponent> attributes;

  // Copy table to top of stack
  lua_pushvalue(state, index);

  // loop over list-like table
  lua_pushnil(state);                   // first key, [table, nil]
  while (lua_next(state, index) != 0) { // [table, key, value]
    // now at -1 is value, -2 is key
    luaL_checktype(state, -1, LUA_TTABLE);

    // Name
    lua_getfield(state, -1, "name");
    if (lua_isstring(state, -1) == 0) {
      return Error::Create("Vertex attribute missing name field.");
    }
    const char *name = luaL_checkstring(state, -1);
    lua_pop(state, 1); // pop name

    // Format
    lua_getfield(state, -1, "format");
    if (lua_isstring(state, -1) == 0) {
      return Error::Create("Vertex attribute missing format field.");
    }
    const char *formatStr = luaL_checkstring(state, -1);
    auto dataFormat = ::Graphics::Format::FromString(formatStr);
    lua_pop(state, 1); // pop format

    // Location
    lua_getfield(state, -1, "location");
    if (lua_isnumber(state, -1) == 0) {
      return Error::Create("Vertex attribute missing location field.");
    }
    int location = static_cast<int>(luaL_checkinteger(state, -1));
    lua_pop(state, 1); // pop location

    attributes.emplace_back(::Graphics::VertexComponent{
        .name = std::string(name),
        .location = static_cast<uint32_t>(location),
        .binding = 0,
        .format = dataFormat,
        .offset = 0,
    });

    lua_pop(state, 1); // pop value, keep key for next iteration
  }
  lua_pop(state, 1); // pop key and table copy

  return attributes;
}

struct ReadInfo {
  int size;
  int count;
  std::function<void(lua_State *, int, std::span<uint8_t>)> read;
};

const inline auto readF32 = [](lua_State *state, int index,
                               std::span<uint8_t> data) -> void {
  auto value = static_cast<float>(luaL_checknumber(state, index));
  std::memcpy(data.data(), &value, sizeof(float));
};

const inline auto readU32 = [](lua_State *state, int index,
                               std::span<uint8_t> data) -> void {
  auto value = static_cast<uint32_t>(luaL_checkinteger(state, index));
  std::memcpy(data.data(), &value, sizeof(uint32_t));
};

const inline auto readI32 = [](lua_State *state, int index,
                               std::span<uint8_t> data) -> void {
  auto value = static_cast<int32_t>(luaL_checkinteger(state, index));
  std::memcpy(data.data(), &value, sizeof(int32_t));
};

const inline auto readF16 = [](lua_State *state, int index,
                               std::span<uint8_t> data) -> void {
  auto value = static_cast<float>(luaL_checknumber(state, index));
  numeric::float16_t half{value};
  std::memcpy(data.data(), &half, sizeof(numeric::float16_t));
};

const inline auto readU16 = [](lua_State *state, int index,
                               std::span<uint8_t> data) -> void {
  auto value = static_cast<uint16_t>(luaL_checkinteger(state, index));
  std::memcpy(data.data(), &value, sizeof(uint16_t));
};

const inline auto readI16 = [](lua_State *state, int index,
                               std::span<uint8_t> data) -> void {
  auto value = static_cast<int16_t>(luaL_checkinteger(state, index));
  std::memcpy(data.data(), &value, sizeof(int16_t));
};

const inline auto readU8 = [](lua_State *state, int index,
                              std::span<uint8_t> data) -> void {
  auto value = static_cast<uint8_t>(luaL_checkinteger(state, index));
  std::memcpy(data.data(), &value, sizeof(uint8_t));
};

const inline auto readI8 = [](lua_State *state, int index,
                              std::span<uint8_t> data) -> void {
  auto value = static_cast<int8_t>(luaL_checkinteger(state, index));
  std::memcpy(data.data(), &value, sizeof(int8_t));
};

constexpr double SnormMax16 = 32767.0;
constexpr double SnormMin16 = -32768.0;

constexpr double UnormMax16 = 65535.0;
constexpr double UnormMin16 = 0.0;

constexpr double SnormMax8 = 127.0;
constexpr double SnormMin8 = -128.0;

constexpr double UnormMax8 = 255.0;
constexpr double UnormMin8 = 0.0;

const inline auto readSnorm16 = [](lua_State *state, int index,
                                   std::span<uint8_t> data) -> void {
  auto value = static_cast<float>(luaL_checknumber(state, index));
  value = std::clamp(value, -1.0F, 1.0F);
  auto snorm = static_cast<int16_t>(std::round(value * SnormMax16));
  std::memcpy(data.data(), &snorm, sizeof(int16_t));
};

const inline auto readUnorm16 = [](lua_State *state, int index,
                                   std::span<uint8_t> data) -> void {
  auto value = static_cast<float>(luaL_checknumber(state, index));
  value = std::clamp(value, 0.0F, 1.0F);
  auto unorm = static_cast<uint16_t>(std::round(value * UnormMax16));
  std::memcpy(data.data(), &unorm, sizeof(uint16_t));
};

const inline auto readSnorm8 = [](lua_State *state, int index,
                                  std::span<uint8_t> data) -> void {
  auto value = static_cast<float>(luaL_checknumber(state, index));
  value = std::clamp(value, -1.0F, 1.0F);
  auto snorm = static_cast<int8_t>(std::round(value * SnormMax8));
  std::memcpy(data.data(), &snorm, sizeof(int8_t));
};

const inline auto readUnorm8 = [](lua_State *state, int index,
                                  std::span<uint8_t> data) -> void {
  auto value = static_cast<float>(luaL_checknumber(state, index));
  value = std::clamp(value, 0.0F, 1.0F);
  auto unorm = static_cast<uint8_t>(std::round(value * UnormMax8));
  std::memcpy(data.data(), &unorm, sizeof(uint8_t));
};

const std::unordered_map<VkFormat, ReadInfo> formatReadInfo = {
    /// Float 32-bit ///
    {VK_FORMAT_R32_SFLOAT, {.size = 4, .count = 1, .read = readF32}},
    {VK_FORMAT_R32G32_SFLOAT, {.size = 4, .count = 2, .read = readF32}},
    {VK_FORMAT_R32G32B32_SFLOAT, {.size = 4, .count = 3, .read = readF32}},
    {VK_FORMAT_R32G32B32A32_SFLOAT, {.size = 4, .count = 4, .read = readF32}},

    /// Unsigned Int 32-bit ///
    {VK_FORMAT_R32_UINT, {.size = 4, .count = 1, .read = readU32}},
    {VK_FORMAT_R32G32_UINT, {.size = 4, .count = 2, .read = readU32}},
    {VK_FORMAT_R32G32B32_UINT, {.size = 4, .count = 3, .read = readU32}},
    {VK_FORMAT_R32G32B32A32_UINT, {.size = 4, .count = 4, .read = readU32}},

    /// Signed Int 32-bit ///
    {VK_FORMAT_R32_SINT, {.size = 4, .count = 1, .read = readI32}},
    {VK_FORMAT_R32G32_SINT, {.size = 4, .count = 2, .read = readI32}},
    {VK_FORMAT_R32G32B32_SINT, {.size = 4, .count = 3, .read = readI32}},
    {VK_FORMAT_R32G32B32A32_SINT, {.size = 4, .count = 4, .read = readI32}},

    /// Float 16-bit ///
    {VK_FORMAT_R16_SFLOAT, {.size = 2, .count = 1, .read = readF16}},
    {VK_FORMAT_R16G16_SFLOAT, {.size = 2, .count = 2, .read = readF16}},
    {VK_FORMAT_R16G16B16_SFLOAT, {.size = 2, .count = 3, .read = readF16}},
    {VK_FORMAT_R16G16B16A16_SFLOAT, {.size = 2, .count = 4, .read = readF16}},

    /// Unsigned Int 16-bit ///
    {VK_FORMAT_R16_UINT, {.size = 2, .count = 1, .read = readU16}},
    {VK_FORMAT_R16G16_UINT, {.size = 2, .count = 2, .read = readU16}},
    {VK_FORMAT_R16G16B16_UINT, {.size = 2, .count = 3, .read = readU16}},
    {VK_FORMAT_R16G16B16A16_UINT, {.size = 2, .count = 4, .read = readU16}},

    /// Signed Int 16-bit ///
    {VK_FORMAT_R16_SINT, {.size = 2, .count = 1, .read = readI16}},
    {VK_FORMAT_R16G16_SINT, {.size = 2, .count = 2, .read = readI16}},
    {VK_FORMAT_R16G16B16_SINT, {.size = 2, .count = 3, .read = readI16}},
    {VK_FORMAT_R16G16B16A16_SINT, {.size = 2, .count = 4, .read = readI16}},

    /// Unsigned Int 8-bit ///
    {VK_FORMAT_R8_UINT, {.size = 1, .count = 1, .read = readU8}},
    {VK_FORMAT_R8G8_UINT, {.size = 1, .count = 2, .read = readU8}},
    {VK_FORMAT_R8G8B8_UINT, {.size = 1, .count = 3, .read = readU8}},
    {VK_FORMAT_R8G8B8A8_UINT, {.size = 1, .count = 4, .read = readU8}},

    /// Signed Int 8-bit ///
    {VK_FORMAT_R8_SINT, {.size = 1, .count = 1, .read = readI8}},
    {VK_FORMAT_R8G8_SINT, {.size = 1, .count = 2, .read = readI8}},
    {VK_FORMAT_R8G8B8_SINT, {.size = 1, .count = 3, .read = readI8}},
    {VK_FORMAT_R8G8B8A8_SINT, {.size = 1, .count = 4, .read = readI8}},

    /// Snorm 16-bit ///
    {VK_FORMAT_R16_SNORM, {.size = 2, .count = 1, .read = readSnorm16}},
    {VK_FORMAT_R16G16_SNORM, {.size = 2, .count = 2, .read = readSnorm16}},
    {VK_FORMAT_R16G16B16_SNORM, {.size = 2, .count = 3, .read = readSnorm16}},
    {VK_FORMAT_R16G16B16A16_SNORM,
     {.size = 2, .count = 4, .read = readSnorm16}},

    /// Unorm 16-bit ///
    {VK_FORMAT_R16_UNORM, {.size = 2, .count = 1, .read = readUnorm16}},
    {VK_FORMAT_R16G16_UNORM, {.size = 2, .count = 2, .read = readUnorm16}},
    {VK_FORMAT_R16G16B16_UNORM, {.size = 2, .count = 3, .read = readUnorm16}},
    {VK_FORMAT_R16G16B16A16_UNORM,
     {.size = 2, .count = 4, .read = readUnorm16}},

    /// Snorm 8-bit ///
    {VK_FORMAT_R8_SNORM, {.size = 1, .count = 1, .read = readSnorm8}},
    {VK_FORMAT_R8G8_SNORM, {.size = 1, .count = 2, .read = readSnorm8}},
    {VK_FORMAT_R8G8B8_SNORM, {.size = 1, .count = 3, .read = readSnorm8}},
    {VK_FORMAT_R8G8B8A8_SNORM, {.size = 1, .count = 4, .read = readSnorm8}},

    /// Unorm 8-bit ///
    {VK_FORMAT_R8_UNORM, {.size = 1, .count = 1, .read = readUnorm8}},
    {VK_FORMAT_R8G8_UNORM, {.size = 1, .count = 2, .read = readUnorm8}},
    {VK_FORMAT_R8G8B8_UNORM, {.size = 1, .count = 3, .read = readUnorm8}},
    {VK_FORMAT_R8G8B8A8_UNORM, {.size = 1, .count = 4, .read = readUnorm8}},

    /// TODO: Add packed formats ///
    // Like rg11b10, rgb9e5, etc.
    // Could be very useful for color packing or other specialized uses
};

auto inline ReadVertex(lua_State *state, int index,
                       ::Graphics::VertexFormat &format,
                       std::span<uint8_t> destination, size_t &writeOffset)
    -> int {

  if (index < 0) {
    index = lua_gettop(state) + index + 1;
  }

  auto offset = 1;

  for (const auto &attribute : format.GetAttributes()) {
    auto iterator = formatReadInfo.find(attribute.format);
    if (iterator == formatReadInfo.end()) {
      return luaL_error(state,
                        "Unsupported vertex attribute format for reading.");
    }

    const auto &readInfo = iterator->second;
    auto readSize = static_cast<size_t>(readInfo.size);

    for (int i = 0; i < readInfo.count; ++i) {
      auto localWriteOffset = writeOffset + (i * readSize);
      lua_rawgeti(state, index, offset++);
      readInfo.read(state, -1, destination.subspan(localWriteOffset, readSize));
      lua_pop(state, 1); // pop value
    }

    writeOffset += readInfo.count * readSize;
  }

  return 0;
}

// "triangles" | "strip" | "lines" | "linestrip" | "points"
inline auto PrimitiveTopologyFromLua(lua_State *state, int index)
    -> VkPrimitiveTopology {
  const char *topologyStr = luaL_checkstring(state, index);

  if (std::strcmp(topologyStr, "triangles") == 0) {
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
  if (std::strcmp(topologyStr, "strip") == 0) {
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  if (std::strcmp(topologyStr, "lines") == 0) {
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  }
  if (std::strcmp(topologyStr, "linestrip") == 0) {
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  }
  if (std::strcmp(topologyStr, "points") == 0) {
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  }
  luaL_error(state, "Unknown primitive topology string: %s", topologyStr);
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

// new mesh
// vertexFormat, vertex count, [topology]
// vertexFormat, bytedata(vertices), [topology]
// vertexFormat, table(vertices), [topology]
// NOLINTNEXTLINE
auto wrap_NewMesh(lua_State *state) -> int {
  auto *ctx = LUA_CK_NULL(::Graphics::GetCurrentGraphicsContext());

  auto attributes = LUA_CK_RES(VertexFormatFromLua(state, 1));
  auto vertexFormat = ::Graphics::VertexFormat(attributes);

  std::span<uint8_t> vertexData;
  std::vector<uint8_t> vertexStorage;
  VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  // Vertex data
  if (lua_type(state, 2) == LUA_TUSERDATA) {
    auto byteData =
        LUA_CK_NULL(LuaWrap::ObjectFromLua<Data::ByteData>(state, 2));

    vertexData = byteData->GetDataSpan();
  } else if (lua_type(state, 2) == LUA_TTABLE) {
    size_t vertexCount = lua_objlen(state, 2);
    size_t stride = vertexFormat.GetStride(0);
    size_t writeOffset = 0;

    vertexStorage.resize(vertexCount * stride);
    vertexData = std::span<uint8_t>(vertexStorage.data(), vertexStorage.size());

    for (int i = 0; i < vertexCount; ++i) {
      lua_rawgeti(state, 2, i + 1); // [table, vertex]
      luaL_checktype(state, -1, LUA_TTABLE);

      ReadVertex(state, -1, vertexFormat, vertexData, writeOffset);

      lua_pop(state, 1); // pop vertex table
    }

    for (int i = 0; i < vertexStorage.size() / sizeof(float); ++i) {
      float value = NAN; // NOLINTNEXTLINE
      std::memcpy(&value, vertexStorage.data() + (i * sizeof(float)),
                  sizeof(float));
    }
  } else {
    return luaL_error(state, "Expected ByteData or table as second argument");
  }

  // Topology
  if (lua_gettop(state) >= 3) {
    topology = PrimitiveTopologyFromLua(state, 3);
  }

  PrintDebug("Creating mesh with {} bytes of vertex data.\n",
             vertexData.size());

  ::Graphics::MeshCreationInfo info{
      .vertexFormat = &vertexFormat,
      .vertexData = {vertexData},
      .vertexCount = vertexData.size() / vertexFormat.GetStride(0),
      .debugName = "LuaMesh",
  };

  auto mesh = LUA_CK_RES(::Graphics::Mesh::Create(*ctx, info));
  LUA_CK_ERR(mesh->SetTopology(topology));

  LuaWrap::PushObject(state, ::Graphics::Mesh::GetType(), mesh.get());

  return 1;
}

auto wrap_GetVertexCount(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));

  lua_pushinteger(state, static_cast<lua_Integer>(mesh->GetVertexCount()));
  return 1;
}

auto wrap_GetIndexCount(lua_State *state) -> int {
  auto mesh = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Mesh>(state, 1));

  lua_pushinteger(state, static_cast<lua_Integer>(mesh->GetIndexCount()));
  return 1;
}
} // namespace Wrap::Graphics::Mesh