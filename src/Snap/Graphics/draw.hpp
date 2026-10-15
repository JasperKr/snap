#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include <cstdint>

namespace Graphics {

// NOLINTNEXTLINE
extern thread_local Ref<Mesh> QuadMesh;

auto CreateQuad01Mesh(const Graphics::GraphicsContext &context)
    -> Result<Ref<Graphics::Mesh>>;

auto BindMesh(const GraphicsContext &context, VkCommandBuffer cmdBuffer,
              const Mesh &mesh) -> Error;

auto Draw(const GraphicsContext &context, Mesh &mesh,
          uint32_t instanceCount = 1) -> Error;

auto Draw(const GraphicsContext &context, const Ref<Texture> &texture,
          uint32_t instanceCount = 1) -> Error;

auto Dispatch(const GraphicsContext &context, const Math::Uvec3 &threadgroups)
    -> Error;

auto DispatchWithin(const GraphicsContext &context, Math::Uvec3 dimensions)
    -> Error;

auto DispatchIndirect(const GraphicsContext &context,
                      const Ref<Buffer> &indirectBuffer, VkDeviceSize offset)
    -> Error;

auto DrawIndirect(const GraphicsContext &context, Mesh &mesh,
                  const Ref<Buffer> &indirectBuffer,
                  VkDeviceSize offset = 0, // NOLINT
                  uint32_t count = 1) -> Error;

// Vertex shader generated versions

auto Draw(const GraphicsContext &context, const VkPrimitiveTopology &topology,
          uint32_t vertexCount, uint32_t instanceCount = 1) -> Error;

auto Draw(const GraphicsContext &context, const Ref<Buffer> &indexBuffer,
          const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error;

} // namespace Graphics