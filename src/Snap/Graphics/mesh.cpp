#include "mesh.hpp"
#include <algorithm>
#include <memory>
#include <mutex>
#include <public/tracy/Tracy.hpp>
#include <string>
#include <sys/types.h>

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "buffer.hpp"
#include <cstdint>
#include <span>
#include <vector>

#include "vulkan/vulkan_core.h"

#include "vertexformat.hpp"

namespace Graphics {

static auto VertexFormatSize(const VertexFormat &format, uint32_t binding)
    -> uint32_t {
  return format.GetStride(binding);
}

auto Mesh::UploadVertices(const GraphicsContext &context, uint32_t binding,
                          const std::span<const uint8_t> &vertices,
                          uint64_t offset) -> Error {
  ZoneScoped;

  std::lock_guard<std::mutex> lock(VertexBuffers.at(binding)->mutex);

  return VertexBuffers.at(binding)->SetData(context, vertices, offset);
}

auto Mesh::UploadIndices(const GraphicsContext &context,
                         const std::span<uint8_t> &indices, uint64_t offset,
                         VkIndexType format) -> Error {
  ZoneScoped;

  std::lock_guard<std::mutex> lock(IndexBuffer->mutex);

  return IndexBuffer->SetData(context, indices, offset);
}

auto Mesh::Create(const GraphicsContext &context, const MeshCreationInfo &info)
    -> Result<Ref<Mesh>> {

  Ref<Mesh> mesh;

  if (!info.vertexData.empty()) {
    mesh = CHECK_RES(CreateFromVertexData(context, info));
  } else {
    ERR_ASSERT_MSG(info.vertexCount > 0, "Vertex count must be greater than 0");
    mesh = CHECK_RES(CreateFromVertexCount(context, info));
  }

  return mesh;
}

auto Mesh::CreateFromVertexData(const GraphicsContext &context,
                                const MeshCreationInfo &info)
    -> Result<Ref<Mesh>> {

  auto mesh = Ref<Mesh>::Make();

  const auto &vertexFormat = *info.vertexFormat;
  const auto &vertexDatas = info.vertexData;
  const auto &debugName = info.debugName;

  auto firstVtxCount = vertexDatas.at(0).size() / vertexFormat.GetStride(0);

  for (size_t i = 1; i < vertexDatas.size(); ++i) {
    auto vtxCount = vertexDatas.at(i).size() / vertexFormat.GetStride(i);
    if (vtxCount != firstVtxCount) {
      return Error::Createf(
          "Vertex data for binding {} has a different vertex count than "
          "binding 0.",
          i);
    }
  }

  mesh->VertexCount = vertexDatas.at(0).size() / vertexFormat.GetStride(0);
  mesh->IndexCount = 0;
  mesh->Format = std::make_unique<VertexFormat>(vertexFormat.Copy());

  auto properties = static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  Graphics::BufferCreationInfo vboCreationInfo = {};
  vboCreationInfo.usage =
      static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  mesh->VertexBuffers.resize(vertexFormat.GetBindingCount());

  for (size_t i = 0; i < vertexFormat.GetBindingCount(); i++) {
    const auto &vertexData = vertexDatas.at(i);
    assert(vertexData.size() % vertexFormat.GetStride(i) == 0);

    vboCreationInfo.properties = properties;
    vboCreationInfo.size = vertexData.size();
    vboCreationInfo.debugName = debugName + " Vertex Buffer";
    mesh->DebugName = debugName;

    mesh->VertexBuffers.at(i) =
        CHECK_RES(Buffer::Create(context, vboCreationInfo));

    CHECK_ERR(mesh->UploadVertices(context, i, vertexData, 0));
  }

  mesh->DrawRange.Offset = 0;
  mesh->DrawRange.Count = mesh->VertexCount;
  mesh->ConstructBindingRanges();

  return mesh;
}

auto Mesh::CreateFromVertexCount(const GraphicsContext &context,
                                 const MeshCreationInfo &info)
    -> Result<Ref<Mesh>> {
  const auto &vertexFormat = *info.vertexFormat;
  const auto &vertexCount = info.vertexCount;
  const auto &debugName = info.debugName;

  std::vector<uint32_t> indexData;

  auto mesh = Ref<Mesh>::Make();

  mesh->VertexCount = vertexCount;
  mesh->Format = std::make_unique<VertexFormat>(vertexFormat.Copy());

  auto properties = static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  uint32_t bindingCount = vertexFormat.GetBindingCount();

  Graphics::BufferCreationInfo vboCreationInfo = {};
  vboCreationInfo.usage =
      static_cast<uint32_t>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

  for (uint32_t i = 0; i < bindingCount; ++i) {
    auto vertexDataSize = vertexCount * VertexFormatSize(vertexFormat, i);

    vboCreationInfo.properties = properties;
    vboCreationInfo.size = vertexDataSize;
    vboCreationInfo.debugName = debugName + " Vertex Buffer";
    mesh->DebugName = debugName;

    auto buffer = CHECK_RES(Buffer::Create(context, vboCreationInfo));

    mesh->VertexBuffers.emplace_back(buffer);
  }

  mesh->DrawRange.Offset = 0;
  mesh->DrawRange.Count = mesh->VertexCount;
  mesh->ConstructBindingRanges();

  return mesh;
}

[[nodiscard]] auto Mesh::GetVertexFormat() -> VertexFormat & { return *Format; }

[[nodiscard]] auto Mesh::GetVertexFormat() const -> const VertexFormat & {
  return *Format;
}

[[nodiscard]] auto Mesh::GetVertexCount() const -> uint32_t {
  return VertexCount;
}

[[nodiscard]] auto Mesh::GetIndexCount() const -> uint32_t {
  return IndexCount;
}

void Mesh::SetDrawRange(MeshDrawRange range) {
  auto maxCount = IndexCount == 0 ? VertexCount : IndexCount;

  assert(range.Offset >= 0 && range.Offset + range.Count <= maxCount);

  DrawRange.Offset = range.Offset;
  DrawRange.Count = range.Count;
}

[[nodiscard]] auto Mesh::GetDrawRange() const -> MeshDrawRange {
  return DrawRange;
}

auto Mesh::SetVertices(const GraphicsContext &context, uint32_t binding,
                       const std::span<const uint8_t> &vertexData,
                       uint64_t offset) -> Error {
  return UploadVertices(context, binding, vertexData, offset);
}

auto Mesh::SetIndices(const GraphicsContext &context,
                      const std::span<uint8_t> &indexData, VkIndexType format,
                      uint32_t offset) -> Error {

  if (format != VK_INDEX_TYPE_UINT16 && format != VK_INDEX_TYPE_UINT32 &&
      format != VK_INDEX_TYPE_UINT8) {
    return Error::Create("Invalid Index Type");
  }

  auto newCount = indexData.size() / GetIndexFormatSize(format);

  if (IndexBuffer.get() == nullptr || IndexBuffer->size < indexData.size()) {
    Graphics::BufferCreationInfo iboCreationInfo = {};
    iboCreationInfo.usage =
        static_cast<uint32_t>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    auto properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    iboCreationInfo.properties = properties;
    iboCreationInfo.size = indexData.size();
    iboCreationInfo.debugName = DebugName + " Index Buffer";

    IndexBuffer = CHECK_RES(Buffer::Create(context, iboCreationInfo));
    IndexCount = newCount;

    DrawRange.Count = IndexCount;
    DrawRange.Offset =
        (std::min)(DrawRange.Offset, static_cast<uint32_t>(IndexCount - 1));
  }

  IndicesFormat = format;

  return UploadIndices(context, indexData, offset, format);
}

auto Mesh::SetVertexBuffer(const Ref<Buffer> &buffer, uint32_t binding)
    -> void {
  VertexBuffers.at(binding) = buffer;
  ConstructBindingRanges();
}

auto Mesh::SetIndexBuffer(const Ref<Buffer> &buffer, VkIndexType format)
    -> Error {

  if (format != VK_INDEX_TYPE_UINT16 && format != VK_INDEX_TYPE_UINT32 &&
      format != VK_INDEX_TYPE_UINT8) {
    return Error::Create("Invalid Index Type");
  }

  IndexBuffer = buffer;
  IndicesFormat = format;

  auto newCount =
      static_cast<uint32_t>(buffer->size / GetIndexFormatSize(format));

  IndexCount = newCount;
  DrawRange.Count = IndexCount;
  DrawRange.Offset =
      (std::min)(DrawRange.Offset, static_cast<uint32_t>(IndexCount - 1));

  return Error::Success();
}

auto Mesh::GetIndexFormat() const -> VkIndexType { return IndicesFormat; }

auto Mesh::GetVertexBuffer(uint32_t binding) const -> Ref<Buffer> {
  return VertexBuffers.at(binding);
}

auto Mesh::GetIndexBuffer() const -> Ref<Buffer> { return IndexBuffer; }

// Disallow: Fan, Geometry, Patch
constexpr std::array<VkPrimitiveTopology, 5> validTopologies = {
    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,     VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

auto Mesh::SetTopology(VkPrimitiveTopology topology) -> Error {
  bool isValid = false;
  for (const auto &validTopology : validTopologies) {
    if (topology == validTopology) {
      isValid = true;
      break;
    }
  }

  if (!isValid) {
    return Error::Create("Invalid primitive topology for Mesh.");
  }

  Topology = topology;

  return Error::Success();
}

auto Mesh::CreateBLAS(const GraphicsContext &context) -> Error {
  ZoneScoped;

  BottomLevelAS = CHECK_RES(BLAS::Create(context, *this));
  // CHECK_ERR(BottomLevelAS->Compact(context));

  return Error::Success();
}

auto Mesh::GetTopology() const -> VkPrimitiveTopology { return Topology; }

auto Mesh::ConstructBindingRanges() -> void {
  size_t index = 0;
  for (auto &buffer : VertexBuffers) {
    assert(buffer.isValid());

    BindingOffsets.at(index) = 0;
    Bindings.at(index++) = buffer->handle;
  }

  auto mapping = Format->GetBindingMapping();
  auto count = Format->GetBindingCount();

  // buffers =  [a, b, c, d, e] size = 5
  // bindings = [a, a, a, _, _, b, _, c] size = 5
  // mapping = [0, 1, 2, 5, 7]
  // i = 0, start = 0
  // 1 < 8 && 0 + 1 == 1 -> i = 1
  // 2 < 8 && 1 + 1 == 2 -> i = 2
  // 3 < 8 && 2 + 1 == 5 -> stop
  // BindingRanges[0] = { .firstBinding = 0, .bindingCount = (2 - 0 + 1) = 3, ... }
  // i++ from for-loop -> i = 3, start = 3
  // 4 < 8 && 5 + 1 == 7 -> stop
  // BindingRanges[1] = { .firstBinding = 5, .bindingCount = (3 - 3 + 1) = 1, ... }
  // i++ from for-loop -> i = 4, start = 4
  // 5 < 8 && 7 + 1 == 8 -> stop
  // BindingRanges[2] = { .firstBinding = 7, .bindingCount = (4 - 4 + 1) = 1, ... }

  BindingRanges.clear();
  for (size_t i = 0; i < count; ++i) {
    size_t start = i;

    // While the next binding is consecutive, keep going.
    while (i + 1 < count && mapping.at(i) + 1 == mapping.at(i + 1)) {
      i++;
    }

    BindingRanges.emplace_back(VertexBindingRange{
        .firstBinding = static_cast<uint32_t>(mapping.at(start)),
        .bindingCount = static_cast<uint32_t>(i - start + 1),
        .bindings = &Bindings.at(start),
        .offsets = &BindingOffsets.at(start),
    });
  }
}

} // namespace Graphics