#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstdint>

namespace Graphics {

// NOLINTNEXTLINE
extern thread_local Ref<Mesh> QuadMesh;

using Ctx = GraphicsContext;

auto CreateQuad01Mesh(const Ctx &context) -> Result<Ref<Graphics::Mesh>>;
auto BindMesh(const Ctx &context, VkCommandBuffer cmdBuffer, const Mesh &mesh) -> Error;
auto Draw(const Ctx &context, Mesh &mesh, uint32_t instanceCount = 1) -> Error;
auto Draw(const Ctx &context, const Ref<Texture> &texture, uint32_t instanceCount = 1) -> Error;
auto Dispatch(const Ctx &context, const Math::Uvec3 &threadgroups) -> Error;
auto DispatchWithin(const Ctx &context, Math::Uvec3 dimensions) -> Error;
auto DispatchIndirect(const Ctx &context, const Ref<Buffer> &buffer, VkDeviceSize offset) -> Error;
auto DrawIndirect(const Ctx &context, Mesh &mesh, const Ref<Buffer> &buffer, VkDeviceSize offset = 0, uint32_t count = 1) -> Error;
auto DrawIndirect(const Ctx &context, const Ref<Buffer> &buffer, VkDeviceSize offset = 0, uint32_t count = 1) -> Error;
auto DrawMeshTasks(const Ctx &context, const Math::Uvec3 &threadgroups) -> Error;
auto DrawMeshTasksIndirect(const Ctx &context, const Ref<Buffer> &buffer, VkDeviceSize offset = 0, uint32_t count = 1) -> Error;
auto DrawMeshTasksIndirectCount(const Ctx &context, const Ref<Buffer> &buffer, uint32_t offset, const Ref<Buffer> &countBuffer, uint32_t countBufferOffset,
                                uint32_t maxDraws, uint32_t stride) -> Error;
auto DrawIndirectCount(const Ctx &context, const Ref<Buffer> &buffer, uint32_t offset, const Ref<Buffer> &countBuffer, uint32_t countBufferOffset,
                       uint32_t maxDraws, uint32_t stride) -> Error;
auto Draw(const GraphicsContext &context, const VkPrimitiveTopology &topology, uint32_t vertexCount, uint32_t instanceCount = 1) -> Error;
auto Draw(const GraphicsContext &context, const Ref<Buffer> &indexBuffer, const VkPrimitiveTopology &topology, uint32_t indexCount, // NOLINT
          uint32_t instanceCount) -> Error;

} // namespace Graphics