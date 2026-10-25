#include "commandState.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/vulkanResource.hpp"
#include "Graphics/vkAccessHelpers.hpp"
#include <utility>
#include <variant>
#include <vector>

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

inline auto GetReadsFromDrawState(DrawState &state,
                                  std::vector<VulkanResource> &reads) -> void {

  for (const auto &image : state.boundImages) {
    if (VkAccessHelpers::IsReadAccess(image.access)) {
      reads.emplace_back(image.resource);
    }
  }

  for (const auto &buffer : state.boundBuffers) {
    if (VkAccessHelpers::IsReadAccess(buffer.access)) {
      reads.emplace_back(buffer.resource);
    }
  }

  for (const auto &accel : state.boundASs) {
    reads.emplace_back(accel.resource);
  }

  for (const auto &vertexBuffer : state.vertexBuffers) {
    if (vertexBuffer != VK_NULL_HANDLE) {
      reads.emplace_back(vertexBuffer);
    }
  }

  if (state.indexBuffer != VK_NULL_HANDLE) {
    reads.emplace_back(state.indexBuffer);
  }
}

auto GetReadsInternal(Command &command, DrawState *drawState,
                      std::vector<VulkanResource> &reads) -> void {
  ZoneScoped;
  if (drawState != nullptr) {
    GetReadsFromDrawState(*drawState, reads);
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
    reads.emplace_back(ImageSubresource(args.texture->imageMemory->image, 0, 1,
                                        0, args.texture->GetMipmapCount()));
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
      reads.emplace_back(
          ImageSubresource(barrier.image, barrier.subresourceRange));
    }

    for (const auto &barrier : args.bufferMemoryBarriers) {
      reads.emplace_back(barrier.buffer);
    }
    break;
  }
  case CommandType::vkCmdBuildAccelerationStructuresKHR: {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    reads.append_range(args.bufferReads);
    break;
  }
  default:
    return;
  }
}

inline auto GetWritesFromDrawState(DrawState &state,
                                   std::vector<VulkanResource> &writes)
    -> void {

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

auto GetWritesInternal(Command &command, DrawState *drawState,
                       std::vector<VulkanResource> &writes) -> void {
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
    writes.emplace_back(ImageSubresource(args.texture->imageMemory->image, 0, 1,
                                         0, args.texture->GetMipmapCount()));
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
      writes.emplace_back(
          ImageSubresource(barrier.image, barrier.subresourceRange));
    }

    for (const auto &barrier : args.bufferMemoryBarriers) {
      writes.emplace_back(barrier.buffer);
    }
    break;
  }
  case CommandType::vkCmdBuildAccelerationStructuresKHR: {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    writes.append_range(args.bufferWrites);
    break;
  }
  default:
    return;
  }
}

// NOLINTNEXTLINE
auto GetResourceAccesses(
    const Command &command,
    std::unordered_map<VulkanResource,
                       std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                       VulkanResourceHash<true>> &accesses) -> void {
  ZoneScoped;

  const auto *drawState = command.GetDrawState();

  if (drawState != nullptr) {
    drawState->GetAccesses(accesses);
  } else if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);

    for (const auto &srcResource : args.srcResources) {
      accesses.emplace(srcResource,
                       std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT));
    }

    for (const auto &dstResource : args.dstResources) {
      accesses.emplace(dstResource,
                       std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT));
    }
  } else if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    accesses.emplace(VulkanResource(ImageSubresource(args.texture)),
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_TRANSFER_BIT));
  } else if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    accesses.emplace(VulkanResource(args.srcBuffer),
                     std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
    accesses.emplace(VulkanResource(args.dstBuffer),
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
  } else if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    for (const auto &resource : args.srcResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    for (const auto &resource : args.dstResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
  } else if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(
                 command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    accesses.emplace(args.srcBuffer,
                     std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
    for (const auto &resource : args.dstResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
  } else if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(
                 command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    for (const auto &resource : args.srcResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    accesses.emplace(args.dstBuffer,
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
  } else if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    accesses.emplace(args.dstBuffer,
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_TRANSFER_BIT));
  } else if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
                 command.data)) {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    for (const auto &read : args.reads) {
      accesses.emplace(
          read, std::make_pair(
                    VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
                        VK_ACCESS_2_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
    }
    for (const auto &write : args.writes) {
      accesses.emplace(
          write, std::make_pair(
                     VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                     VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR));
    }
  } else if (std::holds_alternative<Args::VkCmdPipelineBarrier2>(
                 command.data)) {
    // const auto &args = std::get<Args::VkCmdPipelineBarrier2>(command.data);
    // for (const auto &barrier : args.imageMemoryBarriers) {
    //   accesses.emplace(
    //       ImageSubresource(barrier.image, barrier.subresourceRange),
    //       std::make_pair(barrier.srcAccessMask, barrier.srcStageMask));

    //   accesses.emplace(
    //       ImageSubresource(barrier.image, barrier.subresourceRange),
    //       std::make_pair(barrier.dstAccessMask, barrier.dstStageMask));
    // }
  }
}

} // namespace Graphics