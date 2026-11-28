#include "commandState.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/vulkanResource.hpp"
#include "Graphics/vkAccessHelpers.hpp"
#include "Modules/Helpers/utils.hpp"
#include <unordered_set>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto GetReads(const Command &command) -> const std::vector<VulkanResource> & {
  static const std::vector<VulkanResource> empty{};
  const auto *bound = get_if_derived<BoundResources>(command.data);

  if (bound != nullptr) {
    return bound->reads;
  }

  return empty;
}

auto GetWrites(const Command &command) -> const std::vector<VulkanResource> & {
  static const std::vector<VulkanResource> empty{};
  const auto *bound = get_if_derived<BoundResources>(command.data);

  if (bound != nullptr) {
    return bound->writes;
  }

  return empty;
}

inline auto GetReadsFromDrawState(Command &command, DrawState &state, std::vector<VulkanResource> &reads,
                                  std::unordered_set<VulkanResource, VulkanResourceHash<true>> &knownReads) -> void {

  for (const auto &image : state.boundImages) {
    if (VkAccessHelpers::IsReadAccess(image.access) && !knownReads.contains(image.resource)) {
      reads.emplace_back(image.resource);
      knownReads.emplace(image.resource);
    }
  }

  for (const auto &buffer : state.boundBuffers) {
    if (VkAccessHelpers::IsReadAccess(buffer.access) && !knownReads.contains(buffer.resource)) {
      reads.emplace_back(buffer.resource);
      knownReads.emplace(buffer.resource);
    }
  }

  for (const auto &accel : state.boundASs) {
    if (!knownReads.contains(accel.resource)) {
      reads.emplace_back(accel.resource);
      knownReads.emplace(accel.resource);
    }
  }

  for (const auto &vertexBuffer : state.vertexBuffers) {
    if (vertexBuffer != VK_NULL_HANDLE) {
      reads.emplace_back(vertexBuffer);
    }
  }

  if (state.indexBuffer != VK_NULL_HANDLE) {
    reads.emplace_back(state.indexBuffer);
  }

  switch (command.GetType()) {
  case CommandType::vkCmdDrawIndirect: {
    const auto &args = std::get<Args::VkCmdDrawIndirect>(command.data);
    reads.emplace_back(args.buffer);
    return;
  }
  case CommandType::vkCmdDrawIndexedIndirect: {
    const auto &args = std::get<Args::VkCmdDrawIndexedIndirect>(command.data);
    reads.emplace_back(args.buffer);
    return;
  }
  case CommandType::vkCmdDrawIndirectCount: {
    const auto &args = std::get<Args::VkCmdDrawIndirectCount>(command.data);
    reads.emplace_back(args.buffer);
    reads.emplace_back(args.countBuffer);
    return;
  }
  case CommandType::vkCmdDrawIndexedIndirectCount: {
    const auto &args = std::get<Args::VkCmdDrawIndexedIndirectCount>(command.data);
    reads.emplace_back(args.buffer);
    reads.emplace_back(args.countBuffer);
    return;
  }
  case CommandType::vkCmdDrawMeshTasksIndirect: {
    const auto &args = std::get<Args::VkCmdDrawMeshTasksIndirect>(command.data);
    reads.emplace_back(args.buffer);
    return;
  }
  case CommandType::vkCmdDrawMeshTasksIndirectCount: {
    const auto &args = std::get<Args::VkCmdDrawMeshTasksIndirectCount>(command.data);
    reads.emplace_back(args.buffer);
    reads.emplace_back(args.countBuffer);
    return;
  }
  case CommandType::vkCmdDispatchIndirect: {
    const auto &args = std::get<Args::VkCmdDispatchIndirect>(command.data);
    reads.emplace_back(args.buffer);
    return;
  }
  default:
    return;
  }
}

auto GetReadsInternal(Command &command, DrawState *drawState, std::vector<VulkanResource> &reads,
                      std::unordered_set<VulkanResource, VulkanResourceHash<true>> *knownReads) -> void {
  ZoneScoped;
  if (drawState != nullptr) {
    ASSUME(knownReads != nullptr);
    GetReadsFromDrawState(command, *drawState, reads, *knownReads);
    return;
  }

  switch (command.GetType()) {
  case CommandType::vkCmdBlitImage: {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    reads.append_range(args.srcResources);
    break;
  }
  case CommandType::mipmapTexture: {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    reads.emplace_back(ImageSubresource(args.texture->imageMemory->image, 0, 1, 0, args.texture->GetMipmapCount()));
    break;
  }
  case CommandType::vkCmdCopyBuffer: {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    reads.emplace_back(args.srcBuffer);
    break;
  }
  case CommandType::vkCmdCopyImage: {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    reads.append_range(args.srcResources);
    break;
  }
  case CommandType::vkCmdCopyBufferToImage: {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    reads.emplace_back(args.srcBuffer);
    break;
  }
  case CommandType::vkCmdCopyImageToBuffer: {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    reads.append_range(args.srcResources);
    break;
  }
  case CommandType::vkCmdPipelineBarrier2: {
    const auto &args = std::get<Args::VkCmdPipelineBarrier2>(command.data);
    for (const auto &barrier : args.imageMemoryBarriers) {
      reads.emplace_back(ImageSubresource(barrier.image, barrier.subresourceRange));
    }

    for (const auto &barrier : args.bufferMemoryBarriers) {
      reads.emplace_back(barrier.buffer);
    }
    break;
  }
  case CommandType::vkCmdBuildAccelerationStructuresKHR: {
    const auto &args = std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    reads.append_range(args.bufferReads);
    break;
  }
  default:
    return;
  }
}

inline auto GetWritesFromDrawState(DrawState &state, std::vector<VulkanResource> &writes) -> void {

  for (const auto &buffer : state.boundBuffers) {
    if (VkAccessHelpers::IsWriteAccess(buffer.access)) {
      writes.emplace_back(buffer.resource);
    }
  }

  for (const auto &image : state.boundImages) {
    if (VkAccessHelpers::IsWriteAccess(image.access)) {
      writes.emplace_back(image.resource);
    }
  }
}

auto GetWritesInternal(Command &command, DrawState *drawState, std::vector<VulkanResource> &writes) -> void {
  ZoneScoped;
  if (drawState != nullptr) {
    GetWritesFromDrawState(*drawState, writes);
    return;
  }

  switch (command.GetType()) {
  case CommandType::vkCmdBlitImage: {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    writes.append_range(args.dstResources);
    break;
  }
  case CommandType::mipmapTexture: {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    writes.emplace_back(ImageSubresource(args.texture->imageMemory->image, 0, 1, 0, args.texture->GetMipmapCount()));
    break;
  }
  case CommandType::vkCmdCopyBuffer: {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    writes.emplace_back(args.dstBuffer);
    break;
  }
  case CommandType::vkCmdCopyImage: {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    writes.append_range(args.dstResources);
    break;
  }
  case CommandType::vkCmdCopyBufferToImage: {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    writes.append_range(args.dstResources);
    break;
  }
  case CommandType::vkCmdCopyImageToBuffer: {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    writes.emplace_back(args.dstBuffer);
    break;
  }
  case CommandType::vkCmdFillBuffer: {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    writes.emplace_back(args.dstBuffer);
    break;
  }
  case CommandType::vkCmdPipelineBarrier2: {
    const auto &args = std::get<Args::VkCmdPipelineBarrier2>(command.data);
    for (const auto &barrier : args.imageMemoryBarriers) {
      writes.emplace_back(ImageSubresource(barrier.image, barrier.subresourceRange));
    }

    for (const auto &barrier : args.bufferMemoryBarriers) {
      writes.emplace_back(barrier.buffer);
    }
    break;
  }
  case CommandType::vkCmdBuildAccelerationStructuresKHR: {
    const auto &args = std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    writes.append_range(args.bufferWrites);
    break;
  }
  default:
    return;
  }
}

// NOLINTNEXTLINE
auto GetResourceAccesses(const Command &command,
                         std::unordered_map<VulkanResource, std::pair<VkAccessFlags2, VkPipelineStageFlags2>, VulkanResourceHash<true>> &accesses) -> void {
  ZoneScoped;

  const auto *drawState = command.GetDrawState();

  auto addAccess = [&accesses](const VulkanResource &resource, std::pair<VkAccessFlags2, VkPipelineStageFlags2> access) -> void {
    auto &info = accesses[resource];
    info.first |= access.first;
    info.second |= access.second;
  };

  [[likely]]
  if (drawState != nullptr) {
    drawState->GetAccesses(accesses);
  }

  switch (command.GetType()) {
  case CommandType::vkCmdDrawIndirect: {
    const auto &args = std::get<Args::VkCmdDrawIndirect>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdDrawIndexedIndirect: {
    const auto &args = std::get<Args::VkCmdDrawIndexedIndirect>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdDrawIndirectCount: {
    const auto &args = std::get<Args::VkCmdDrawIndirectCount>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    addAccess(VulkanResource(args.countBuffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdDrawIndexedIndirectCount: {
    const auto &args = std::get<Args::VkCmdDrawIndexedIndirectCount>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    addAccess(VulkanResource(args.countBuffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdDrawMeshTasksIndirect: {
    const auto &args = std::get<Args::VkCmdDrawMeshTasksIndirect>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdDrawMeshTasksIndirectCount: {
    const auto &args = std::get<Args::VkCmdDrawMeshTasksIndirectCount>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    addAccess(VulkanResource(args.countBuffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdDispatchIndirect: {
    const auto &args = std::get<Args::VkCmdDispatchIndirect>(command.data);
    addAccess(VulkanResource(args.buffer), std::make_pair(VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT, VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT));
    return;
  }
  case CommandType::vkCmdBlitImage: {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);

    for (const auto &srcResource : args.srcResources) {
      addAccess(srcResource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT));
    }

    for (const auto &dstResource : args.dstResources) {
      addAccess(dstResource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT));
    }
    return;
  }
  case CommandType::vkCmdCopyBuffer: {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    addAccess(VulkanResource(args.srcBuffer), std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    addAccess(VulkanResource(args.dstBuffer), std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    return;
  }
  case CommandType::vkCmdCopyImage: {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    for (const auto &resource : args.srcResources) {
      addAccess(resource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    for (const auto &resource : args.dstResources) {
      addAccess(resource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    return;
  }
  case CommandType::vkCmdCopyBufferToImage: {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    addAccess(args.srcBuffer, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    for (const auto &resource : args.dstResources) {
      addAccess(resource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    return;
  }
  case CommandType::vkCmdCopyImageToBuffer: {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    for (const auto &resource : args.srcResources) {
      addAccess(resource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    addAccess(args.dstBuffer, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT));
    return;
  }
  case CommandType::mipmapTexture: {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    addAccess(VulkanResource(ImageSubresource(args.texture)), std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT));
    return;
  }
  case CommandType::vkCmdFillBuffer: {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    addAccess(args.dstBuffer, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT));
    return;
  }
  case CommandType::vkCmdBuildAccelerationStructuresKHR: {
    const auto &args = std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    for (const auto &read : args.reads) {
      addAccess(read, std::make_pair(VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT,
                                     VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
    }
    for (const auto &write : args.writes) {
      addAccess(write, std::make_pair(VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
    }
    return;
  }
  default:
    return;
  }
}

} // namespace Graphics