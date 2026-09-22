#include "draw.hpp"
#include "Graphics/Buffers/uniform.hpp"

#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/dynamicRendering.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/snapshot.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>

#include <vulkan/vulkan_core.h>

namespace Graphics {

// NOLINTNEXTLINE
thread_local Ref<Mesh> QuadMesh;

struct FormatDefault2D {
  float position[2]; // NOLINT
  float texCoord[2]; // NOLINT
  uint32_t color;
};

auto CreateQuad01Mesh(const Graphics::GraphicsContext &context)
    -> Result<Ref<Graphics::Mesh>> {
  std::vector<FormatDefault2D> vertices{4};
  std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

  vertices[0].position[0] = 0.0F;
  vertices[0].position[1] = 0.0F;
  vertices[0].texCoord[0] = 0.0F;
  vertices[0].texCoord[1] = 0.0F;
  vertices[0].color = ~0U;

  vertices[1].position[0] = 1.0F;
  vertices[1].position[1] = 0.0F;
  vertices[1].texCoord[0] = 1.0F;
  vertices[1].texCoord[1] = 0.0F;
  vertices[1].color = ~0U;

  vertices[2].position[0] = 1.0F;
  vertices[2].position[1] = 1.0F;
  vertices[2].texCoord[0] = 1.0F;
  vertices[2].texCoord[1] = 1.0F;
  vertices[2].color = ~0U;

  vertices[3].position[0] = 0.0F;
  vertices[3].position[1] = 1.0F;
  vertices[3].texCoord[0] = 0.0F;
  vertices[3].texCoord[1] = 1.0F;
  vertices[3].color = ~0U;

  static Graphics::VertexFormat vertexFormat({
      Graphics::VertexComponent{
          .name = "Position",
          .location = 0,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      Graphics::VertexComponent{
          .name = "TexCoord",
          .location = 1,
          .binding = 0,
          .format = VK_FORMAT_R32G32_SFLOAT,
      },
      Graphics::VertexComponent{
          .name = "Color",
          .location = 2,
          .binding = 0,
          .format = VK_FORMAT_R8G8B8A8_UNORM,
      },
  });

  // NOLINTNEXTLINE; Reinterpret cast is necessary here
  auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(vertices.data()),
                                 vertexFormat.GetBindings()[0].stride *
                                     vertices.size());

  assert(sizeof(FormatDefault2D) == vertexFormat.GetBindings()[0].stride);

  MeshCreationInfo info{
      .vertexFormat = &vertexFormat,
      .vertexData = {span},
      .vertexCount = vertices.size(),
      .debugName = "Quad01Mesh",
  };

  auto mesh = CHECK_RES(Graphics::Mesh::Create(context, info));

  CHECK_ERR(mesh->SetVertices(context, 0, span));

  auto indexSpan = std::span<uint8_t>( // NOLINTNEXTLINE
      reinterpret_cast<uint8_t *>(indices.data()),
      indices.size() * Graphics::GetIndexFormatSize(VK_INDEX_TYPE_UINT32));

  CHECK_ERR(mesh->SetIndices(context, indexSpan, VK_INDEX_TYPE_UINT32));

  return mesh;
}

using namespace Snapshot;

// NOLINTNEXTLINE
auto BindMesh(const GraphicsContext &context, const Mesh &mesh) -> Error {
  ZoneScoped;
  auto count =
      mesh.GetIndexCount() > 0 ? mesh.GetIndexCount() : mesh.GetVertexCount();

#ifndef NDEBUG
  switch (mesh.GetTopology()) {
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    if (count % 2 != 0) {
      return Error::Create(
          "Line List topology requires an even number of vertices.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    if (count < 2) {
      return Error::Create("Line Strip topology requires at least 2 vertices.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    if (count % 3 != 0) {
      return Error::Create("Triangle List topology requires vertex count to be "
                           "a multiple of 3.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    if (count < 3) {
      return Error::Create(
          "Triangle Strip topology requires at least 3 vertices.");
    }
    break;
  case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
  default:
    break;
  }
#endif
  auto &threadContext = GetThreadContext();
  auto vertexBuffer = mesh.GetVertexBuffer();

  assert(vertexBuffer.isValid());
  ASSUME(vertexBuffer.isValid());

  auto *commandBuffer = CHECK_NULL(GetVirtualCommandBuffer());

  if (mesh.GetIndexCount() > 0) {
    auto indexBuffer = mesh.GetIndexBuffer();

    assert(indexBuffer.isValid());

    std::lock_guard<std::mutex> lock(indexBuffer->mutex);

    assert(indexBuffer->handle != VK_NULL_HANDLE);

    commandBuffer->BindIndexBuffer(
        {indexBuffer->handle, 0, mesh.GetIndexFormat()});
  } else {
    commandBuffer->BindIndexBuffer({nullptr, 0, VK_INDEX_TYPE_UINT32});
  }

  const auto &bindings = mesh.GetBindingRanges();
  for (const auto &binding : bindings) {
    commandBuffer->BindVertexBuffers({binding.firstBinding,
                                      binding.bindingCount, binding.bindings,
                                      binding.offsets});
  }

  return Error::Success();
}

inline auto IsHazard(const Ref<Graphics::Texture> &first,
                     const Ref<Graphics::Texture> &second) {
  if (first->imageMemory->image != second->imageMemory->image) {
    return false;
  }

  // Check if the two textures overlap in mip levels or array layers
  auto firstMipRange = std::make_pair(first->baseMipLevel,
                                      first->baseMipLevel + first->levelCount);
  auto secondMipRange = std::make_pair(
      second->baseMipLevel, second->baseMipLevel + second->levelCount);

  auto firstLayerRange = std::make_pair(
      first->baseArrayLayer, first->baseArrayLayer + first->layerCount);
  auto secondLayerRange = std::make_pair(
      second->baseArrayLayer, second->baseArrayLayer + second->layerCount);

  bool mipOverlap = firstMipRange.second > secondMipRange.first &&
                    secondMipRange.second > firstMipRange.first;
  bool layerOverlap = firstLayerRange.second > secondLayerRange.first &&
                      secondLayerRange.second > firstLayerRange.first;

  return mipOverlap && layerOverlap;
}

inline auto InsertTextureBarriers(const GraphicsContext &context,
                                  const Ref<Shader> &shader) -> Error {

  for (auto &texturePair : shader->GetState().userBoundTextures) {
    auto &texture = texturePair.second;
    auto key = texturePair.first;

    texture.first->MarkUse();

    const auto *infoResult = shader->GetSlotDescription(key);
    if (infoResult == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound texture slot.");
    }

    if (!infoResult->Is<Reflect::SamplerInfo>()) {
      return Error::Createf(
          "Expected sampler info for bound texture slot. Got: {}",
          infoResult->ToString());
    }

    const auto &info = infoResult->GetInfo<Reflect::SamplerInfo>();

    if (info.accessFlags == 0) {
      PrintWarning("Texture access type is Unknown for slang access: {}, "
                   "skipping barrier.",
                   static_cast<uint32_t>(info.access));
      continue;
    }

#ifndef NDEBUG
    if (RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      const auto &targets = RenderState::GetRenderTargets();
      for (const auto &target : targets) {
        if (IsHazard(texture.first, target.texture)) {
          auto debugname = texture.first->GetDebugName();
          return Error::Createf(
              "Texture {} is bound as a render target, but is also used as a "
              "shader "
              "resource. This is not supported and may indicate a bug in the "
              "application.",
              debugname);
        }
      }
    }
#endif
  }

  return {};
}

inline auto InsertBufferBarriers(const GraphicsContext &context,
                                 const Ref<Shader> &shader) -> Error {

  for (auto &bufferPair : shader->GetState().userBoundBuffers) {
    auto &buffer = bufferPair.second;
    auto key = bufferPair.first;

    buffer.first->MarkUse();

    const auto *slotInfo = shader->GetSlotDescription(key);
    if (slotInfo == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound buffer slot.");
    }
    if (!slotInfo->Is<Reflect::BufferInfo>()) {
      return Error::Create("Expected buffer info for bound buffer slot.");
    }

    const auto &info = slotInfo->GetInfo<Reflect::BufferInfo>();

    VkAccessFlags2 access = info.accessFlags;

    if (access == 0 && info.access != SLANG_RESOURCE_ACCESS_NONE) {
      PrintWarning("Buffer access type is Unknown for slang access: {}, "
                   "skipping barrier.",
                   static_cast<uint32_t>(info.access));
      continue;
    }

    auto stages = shader->combinedPipelineStages;
  }

  const auto &globalUBO =
      GetGlobalUniformBuffer(context.frameIndex).GetBuffer();
  globalUBO->MarkUse();

  return {};
}

inline auto InsertAccelerationStructureBarriers(const GraphicsContext &context,
                                                const Ref<Shader> &shader)
    -> Error {

  for (auto &asPair : shader->GetState().userBoundAccelerationStructures) {
    auto &structure = asPair.second;
    auto key = asPair.first;

    structure.first->MarkUse();

    const auto *slotInfo = shader->GetSlotDescription(key);
    if (slotInfo == nullptr) {
      return Error::Create(
          "Failed to get slot description for bound acceleration structure "
          "slot.");
    }
    if (!slotInfo->Is<Reflect::AccelerationStructureInfo>()) {
      return Error::Create(
          "Expected acceleration structure info for bound acceleration "
          "structure slot.");
    }

    const auto &info = slotInfo->GetInfo<Reflect::AccelerationStructureInfo>();

    VkAccessFlags2 access = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    auto stages = shader->combinedPipelineStages;
  }

  return {};
}

inline auto InsertResourceBarriers(const GraphicsContext &context) -> Error {
  auto shader = RenderState::GetShader();

  CHECK_ERR(InsertTextureBarriers(context, shader));
  CHECK_ERR(InsertBufferBarriers(context, shader));
  CHECK_ERR(InsertAccelerationStructureBarriers(context, shader));

  return Error::Success();
}

auto Draw(const GraphicsContext &context, Mesh &mesh, uint32_t instanceCount)
    -> Error {
  ZoneScoped;

  auto *commandBuffer = CHECK_NULL(GetVirtualCommandBuffer());

  CHECK_ERR(BindMesh(context, mesh));

  RenderState::SetTopology(mesh.GetTopology());

  auto &vertexFormat = mesh.GetVertexFormat();
  vertexFormat.BindDynamicInputState(commandBuffer);

  CHECK_ERR(InsertResourceBarriers(context));

  auto vertexCount = mesh.GetVertexCount();

  {
    ZoneScopedN("Vk Draw");
    RenderState::CurrentStats.drawCalls++;
    RenderState::CurrentStats.triangleCount +=
        static_cast<uint64_t>(mesh.GetIndexCount() * instanceCount);
    RenderState::CurrentStats.instanceCount += instanceCount;

    const MeshDrawRange &range = mesh.GetDrawRange();

    if (mesh.GetIndexCount() > 0) {
      CHECK_ERR(commandBuffer->DrawIndexed(
          {range.Count, instanceCount, range.Offset, 0, 0}));

#if Enable_Snapshots
      CaptureEvent(DrawIndexedEvent(mesh.GetIndexCount(), instanceCount,
                                    range.Offset, 0, 0));
#endif
    } else {
      CHECK_ERR(
          commandBuffer->Draw({range.Count, instanceCount, range.Offset, 0}));

#if Enable_Snapshots
      CaptureEvent(DrawEvent(vertexCount, instanceCount, 0, 0));
#endif
    }
  }

  {
    ZoneScopedN("Resource management")

    {
      const auto &vertexBuffer = mesh.GetVertexBuffer();
      // std::lock_guard<std::mutex> lock(vertexBuffer->mutex);
      vertexBuffer->MarkUse();
    }

    if (mesh.GetIndexCount() > 0) {
      const auto &indexBuffer = mesh.GetIndexBuffer();
      // std::lock_guard<std::mutex> lock(indexBuffer->mutex);
      indexBuffer->MarkUse();
    }
  }

  return Error::Success();
}

auto Draw(const GraphicsContext &context, Texture &texture,
          uint32_t instanceCount) -> Error {
  ZoneScoped;

  if (!QuadMesh.isValid()) {
    QuadMesh = CHECK_RES(CreateQuad01Mesh(context));
  }

  auto shader = RenderState::GetShader();

  CHECK_ERR(shader->Send({"MainTexture"}, Ref<Texture>(&texture)));

  return Draw(context, *QuadMesh, instanceCount);
}

auto Dispatch(const GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetVirtualCommandBuffer();

  ERR_ASSERT(commandBuffer != nullptr);

  ERR_ASSERT(RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_COMPUTE);
  CHECK_ERR(InsertResourceBarriers(context));

  {
    ZoneScopedN("Vk Dispatch");

    RenderState::CurrentStats.dispatchCalls++;
    CHECK_ERR(commandBuffer->Dispatch(
        {threadgroups.x, threadgroups.y, threadgroups.z}));
  }

#if Enable_Snapshots
  CaptureEvent(DispatchEvent(threadgroups.x, threadgroups.y, threadgroups.z));
#endif

  return Error::Success();
}

auto DispatchWithin(const GraphicsContext &context, Math::Uvec3 dimensions)
    -> Error {
  ZoneScoped;

  const auto &shader = RenderState::GetUserShader();
  if (shader == nullptr) {
    return Error::Create("No shader bound for dispatch call.");
  }

  const auto &threadgroupSize = CHECK_RES(shader->GetThreadgroupSize());

  // Allow passing in 0.
  dimensions.x = std::max(dimensions.x, 1U);
  dimensions.y = std::max(dimensions.y, 1U);
  dimensions.z = std::max(dimensions.z, 1U);

  Math::Uvec3 threadgroups{
      Utils::CeilDiv(dimensions.x, threadgroupSize.x),
      Utils::CeilDiv(dimensions.y, threadgroupSize.y),
      Utils::CeilDiv(dimensions.z, threadgroupSize.z),
  };

  return Dispatch(context, threadgroups);
}

auto DispatchIndirect(const GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error {
  ZoneScoped;
  auto *commandBuffer = GetVirtualCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for dispatch indirect.");
  }

  ERR_ASSERT(RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_COMPUTE);

  // Also stops rendering if we are bound to a compute shader.

  CHECK_ERR(InsertResourceBarriers(context));

  RenderState::CurrentStats.dispatchCalls++;
  CHECK_ERR(commandBuffer->DispatchIndirect({indirectBuffer->handle, offset}));

#if Enable_Snapshots
  CaptureEvent(DispatchIndirectEvent(indirectBuffer->handle, offset));
#endif

  return Error::Success();
}

auto DrawIndirect(const GraphicsContext &context, Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset, // NOLINT
                  uint32_t count) -> Error {
  ZoneScoped;
  auto *commandBuffer = GetVirtualCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw indirect.");
  }

  CHECK_ERR(BindMesh(context, mesh));
  ERR_ASSERT(RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS);
  auto &vertexFormat = mesh.GetVertexFormat();
  vertexFormat.BindDynamicInputState(commandBuffer);
  RenderState::SetTopology(mesh.GetTopology());

  CHECK_ERR(InsertResourceBarriers(context));

  if (offset % 4 != 0) {
    return Error::Create(
        "Offset for vkCmdDrawIndirect must be a multiple of 4.");
  }

  RenderState::CurrentStats.drawCalls++;
  RenderState::CurrentStats.triangleCount +=
      static_cast<uint64_t>(mesh.GetIndexCount() * count);
  RenderState::CurrentStats.instanceCount += count;

  CHECK_ERR(commandBuffer->DrawIndirect(
      {indirectBuffer->handle, offset, count, sizeof(VkDrawIndirectCommand)}));

#if Enable_Snapshots
  CaptureEvent(DrawIndirectEvent(indirectBuffer->handle, offset, count,
                                 sizeof(VkDrawIndirectCommand)));
#endif

  {
    std::lock_guard<std::mutex> lock(mesh.GetVertexBuffer()->mutex);
    mesh.GetVertexBuffer()->MarkUse();
  }
  if (mesh.GetIndexCount() > 0) {
    std::lock_guard<std::mutex> lock(mesh.GetIndexBuffer()->mutex);
    mesh.GetIndexBuffer()->MarkUse();
  }
  {
    std::lock_guard<std::mutex> lock(indirectBuffer->mutex);
    indirectBuffer->MarkUse();
  }

  return Error::Success();
}

// Vertex shader generated versions

auto Draw(const GraphicsContext &context, const VkPrimitiveTopology &topology,
          uint32_t vertexCount, uint32_t instanceCount) -> Error { // NOLINT
  ZoneScoped;
  auto *commandBuffer = GetVirtualCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  ERR_ASSERT(RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS);
  commandBuffer->SetVertexInputEXT({0, nullptr, 0, nullptr});
  commandBuffer->BindVertexBuffers({0, 0, nullptr, nullptr});
  commandBuffer->BindIndexBuffer({nullptr, 0, VK_INDEX_TYPE_UINT32});
  RenderState::SetTopology(topology);

  CHECK_ERR(InsertResourceBarriers(context));

  RenderState::CurrentStats.drawCalls++;
  RenderState::CurrentStats.triangleCount +=
      static_cast<uint64_t>(vertexCount * instanceCount);
  RenderState::CurrentStats.instanceCount += instanceCount;

  CHECK_ERR(commandBuffer->Draw({vertexCount, instanceCount, 0, 0}));

#if Enable_Snapshots
  CaptureEvent(DrawEvent(vertexCount, instanceCount, 0, 0));
#endif

  return Error::Success();
}

auto Draw(const GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error {
  ZoneScoped;
  auto *commandBuffer = GetVirtualCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Failed to get command buffer for draw call.");
  }

  ERR_ASSERT(RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS);
  commandBuffer->SetVertexInputEXT({0, nullptr, 0, nullptr});
  commandBuffer->BindVertexBuffers({0, 0, nullptr, nullptr});
  RenderState::SetTopology(topology);

  CHECK_ERR(InsertResourceBarriers(context));

  {
    std::lock_guard<std::mutex> lock(indexBuffer->mutex);

    commandBuffer->BindIndexBuffer(
        {indexBuffer->handle, 0, VK_INDEX_TYPE_UINT32});
    indexBuffer->MarkUse();
  }

  RenderState::CurrentStats.drawCalls++;
  RenderState::CurrentStats.triangleCount +=
      static_cast<uint64_t>(indexCount * instanceCount);
  RenderState::CurrentStats.instanceCount += instanceCount;

  CHECK_ERR(commandBuffer->DrawIndexed({indexCount, instanceCount, 0, 0, 0}));

#if Enable_Snapshots
  CaptureEvent(DrawIndexedEvent(indexCount, instanceCount, 0, 0, 0));
#endif

  return Error::Success();
}

} // namespace Graphics