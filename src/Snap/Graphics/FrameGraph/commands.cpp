#include "commands.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/FrameGraph/virtualState.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/renderState.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "commandState.hpp"
#include "dynamicRendering.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <public/tracy/Tracy.hpp>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

ImageSubresource::ImageSubresource(const Ref<Texture> &texture)
    : layerStart(
          static_cast<uint16_t>(texture->subresourceRange.baseArrayLayer)),
      layerCount(static_cast<uint16_t>(texture->subresourceRange.layerCount)),
      mipStart(static_cast<uint16_t>(texture->subresourceRange.baseMipLevel)),
      mipCount(static_cast<uint16_t>(texture->subresourceRange.levelCount)),
      image(texture->imageMemory->image) {}

ImageSubresource::ImageSubresource(const Texture *texture)
    : layerStart(
          static_cast<uint16_t>(texture->subresourceRange.baseArrayLayer)),
      layerCount(static_cast<uint16_t>(texture->subresourceRange.layerCount)),
      mipStart(static_cast<uint16_t>(texture->subresourceRange.baseMipLevel)),
      mipCount(static_cast<uint16_t>(texture->subresourceRange.levelCount)),
      image(texture->imageMemory->image) {}

struct SyncFlags {
  VkAccessFlags2 access = 0;
  VkPipelineStageFlags2 pipelines = 0;

  auto operator|=(const SyncFlags &other) -> SyncFlags & {
    access |= other.access;
    pipelines |= other.pipelines;
    return *this;
  }

  [[nodiscard]] auto Get() -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
    return {access, pipelines};
  }
};

void VCmdThreadInfo::ThreadMain(VirtualCommandBuffer *vcmd) {
  auto &info = vcmd->threadInfo;

  while (!info.shutdown.load()) {
    {
      info.mainToWorker.Wait();

      std::lock_guard<std::mutex> lock(info.commandsQueueMutex);

      if (info.shutdown.load()) {
        return;
      }

      assert(info.queuedCommands.empty());
      info.queuedCommands.append_range(info.commandsQueue);
      info.commandsQueue.clear();
    }

    {
      ZoneScopedN("Update resource accesses loop");

      for (const uint16_t commandID : info.queuedCommands) {
        auto &command = vcmd->commandStorage.at(commandID);

        BoundResources *boundState = nullptr;
        if (command.parentID != UINT16_MAX) {
          boundState = get_if_derived<BoundResources>(
              vcmd->commandStorage.at(command.parentID).data);
        } else {
          boundState = get_if_derived<BoundResources>(command.data);
        }

        GetResourceAccesses(command, boundState->accesses);
      }

      info.queuedCommands.clear();
    }

    info.workerToMain.Notify();
  }
}

void VCmdThreadInfo::Initialize(VirtualCommandBuffer *vcmd) {
  parent = vcmd;
  thread = std::thread(ThreadMain, vcmd);
  thread.detach();
}

void VCmdThreadInfo::AddCommand(uint16_t commandID) {
  ZoneScopedN("Queue command to thread");

  assert(!started);
  commandsQueueBacklog.emplace_back(commandID);
}

auto DrawState::GetAccesses(
    std::unordered_map<VulkanResource,
                       std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                       VulkanResourceHash<true>> &accesses) const -> void {

  for (const auto &bound : boundImages) {
    accesses.emplace(bound.resource,
                     std::make_pair(bound.access, bound.pipelines));
  }

  for (const auto &bound : boundBuffers) {
    accesses.emplace(bound.resource,
                     std::make_pair(bound.access, bound.pipelines));
  }

  for (const auto &bound : boundASs) {
    accesses.emplace(bound.resource,
                     std::make_pair(bound.access, bound.pipelines));
  }

  for (const VkBuffer buffer : vertexBuffers) {
    accesses.emplace(
        VulkanResource(buffer),
        std::make_pair(VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                       VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT));
  }

  if (indexBuffer != nullptr) {
    accesses.emplace(VulkanResource(indexBuffer),
                     std::make_pair(VK_ACCESS_2_INDEX_READ_BIT,
                                    VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT));
  }
}

auto VirtualState::GetHash() const -> uint64_t {
  if (dirty) {
    Hash::Hasher hasher{};

    hash = hasher.Get();

    dirty = false;
  }

  return hash;
}

auto DrawState::Initialize(CommandType type) -> Error {
  ZoneScoped;

  const auto &shader = RenderState::GetShader();
  const auto &context = *GetCurrentGraphicsContext();
  const auto &ctx = GetThreadContext();
  const auto &shaderState = shader->GetState();
  const auto &pipelines = shader->combinedPipelineStages;

  assert(shader.get() != nullptr);

  scissor = RenderState::GetScissor();
  viewport = RenderState::GetClippedViewport();
  cullMode = RenderState::GetCullMode();
  depthTestEnable = RenderState::TopOfStack->depthTestEnable;
  depthWriteEnable = RenderState::TopOfStack->depthWriteEnable;
  depthCompareOp = RenderState::TopOfStack->depthCompareOp;
  frontFace = RenderState::GetWindingOrder();

  boundImages.reserve(shaderState.userBoundTextures.size());
  boundBuffers.reserve(shaderState.userBoundBuffers.size());
  boundASs.reserve(shaderState.userBoundAccelerationStructures.size());

  boundImages.clear();
  boundBuffers.clear();
  boundASs.clear();

  for (const auto &texture : shaderState.userBoundTextures) {
    boundImages.emplace_back(BoundResource{
        .resource = ImageSubresource{texture.second.resource},
        .access = texture.second.access,
        .pipelines = pipelines,
    });
  }

  for (const auto &buffer : shaderState.userBoundBuffers) {
    boundBuffers.emplace_back(BoundResource{
        .resource = buffer.second.resource->handle,
        .access = buffer.second.access,
        .pipelines = pipelines,
    });
  }

  for (const auto &accelerationStructure :
       shaderState.userBoundAccelerationStructures) {
    boundASs.emplace_back(BoundResource{
        .resource =
            accelerationStructure.second.resource->GetAccelerationStructure(),
        .access = accelerationStructure.second.access,
        .pipelines = pipelines,
    });
  }

  VirtualState &currentState = GetVirtualCommandBuffer()->currentState;

  if (RenderState::GetBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    vertexBuffers.resize(currentState.vertexBuffers.size());
    memcpy(vertexBuffers.data(),              // NOLINT
           currentState.vertexBuffers.data(), // NOLINT
           currentState.vertexBuffers.size() * sizeof(VkBuffer));

    vertexBufferOffsets.resize(currentState.vertexBufferOffsets.size());
    memcpy(vertexBufferOffsets.data(), currentState.vertexBufferOffsets.data(),
           currentState.vertexBufferOffsets.size() * sizeof(VkDeviceSize));

    indexBuffer = currentState.indexBuffer;
    indexBufferOffset = currentState.indexBufferOffset;
    indexType = currentState.indexType;

    bindingDescriptions = currentState.bindingDescriptions;
    attributeDescriptions = currentState.attributeDescriptions;
  } else {
    vertexBuffers.clear();
    indexBuffer = nullptr;

    bindingDescriptions.clear();
    attributeDescriptions.clear();
  }

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);
  auto offset = buffer.GetOffset();

  ERR_ASSERT(offset < UINT32_MAX);
  assert(shader->GetState().uniforms.size() == shader->uniformsSize);
  CHECK_ERR(buffer.Write(context, shader->GetState().uniforms));

  uboAddress = CHECK_RES(
      buffer.GetBuffer()->GetAddress(context, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     offset, shader->uniformsSize));

  return {};
}

inline auto GetNewEmptyArgs(CommandType type) -> ArgVariants {
  ZoneScoped;

  switch (type) {
  case CommandType::vkCmdDraw:
    return Args::VkCmdDraw();
  case CommandType::vkCmdDrawIndexed:
    return Args::VkCmdDrawIndexed();
  case CommandType::vkCmdDrawIndirect:
    return Args::VkCmdDrawIndirect();
  case CommandType::vkCmdDrawIndexedIndirect:
    return Args::VkCmdDrawIndexedIndirect();
  case CommandType::vkCmdDispatch:
    return Args::VkCmdDispatch();
  case CommandType::vkCmdDispatchIndirect:
    return Args::VkCmdDispatchIndirect();
  case CommandType::vkCmdBlitImage:
    return Args::VkCmdBlitImage();
  case CommandType::vkCmdCopyBuffer:
    return Args::VkCmdCopyBuffer();
  case CommandType::vkCmdCopyImage:
    return Args::VkCmdCopyImage();
  case CommandType::vkCmdCopyBufferToImage:
    return Args::VkCmdCopyBufferToImage();
  case CommandType::vkCmdCopyImageToBuffer:
    return Args::VkCmdCopyImageToBuffer();
  case CommandType::mipmapTexture:
    return Args::MipmapTexture();
  case CommandType::vkCmdFillBuffer:
    return Args::VkCmdFillBuffer();
  case CommandType::vkCmdBuildAccelerationStructuresKHR:
    return Args::VkCmdBuildAccelerationStructuresKHR();
  case CommandType::vkCmdCopyAccelerationStructureKHR:
    return Args::VkCmdCopyAccelerationStructureKHR();
  case CommandType::vkCmdResetQueryPool:
    return Args::VkCmdResetQueryPool();
  case CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR:
    return Args::VkCmdWriteAccelerationStructuresPropertiesKHR();
  case CommandType::vkCmdClearAttachments:
    return Args::VkCmdClearAttachments();
  case CommandType::vkCmdPipelineBarrier2:
    return Args::VkCmdPipelineBarrier2();
  case CommandType::renderPass:
    return RenderPass();
  }

  assert(false);
}

auto VirtualCommandBuffer::NewRenderpass() -> void {
  currentRenderPass = GetNewCommand(CommandType::renderPass);
  Command &renderpassCommand = GetCommand(currentRenderPass);
  auto &newPassData = std::get<RenderPass>(renderpassCommand.data);
  newPassData.commands.clear();

  auto &virtualState = newPassData.state;
  virtualState.MarkUpdated();

  virtualState.shader = RenderState::GetShader();
  virtualState.bindPoint = RenderState::GetBindPoint();

  virtualState.stencilTestEnable = RenderState::TopOfStack->stencilTestEnable;
  virtualState.polygonMode = RenderState::GetPolygonMode();
  virtualState.primitiveTopology = RenderState::GetTopology();

  virtualState.colorBlendEquations.resize(
      RenderState::TopOfStack->colorBlendEquations.size());
  memcpy(virtualState.colorBlendEquations.data(),
         RenderState::TopOfStack->colorBlendEquations.data(),
         RenderState::TopOfStack->colorBlendEquations.size() *
             sizeof(VkColorBlendEquationEXT));

  VkAccessFlags2 depthAccess = VK_ACCESS_2_NONE;
  VkPipelineStageFlags2 depthPipelines = VK_PIPELINE_STAGE_2_NONE;

  // clang-format off
  depthAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT ;
  depthAccess |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  depthPipelines |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
  depthPipelines |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  // clang-format on

  virtualState.colorAttachments.clear();

  for (const auto &target : RenderState::TopOfStack->colorAttachments) {
    auto attachment = BoundResource{
        .resource = ImageSubresource{target.texture},
        .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                  VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .pipelines = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    virtualState.colorAttachments.emplace_back(target);

    if (!RenderState::TopOfStack->rendertargetsUpdated) {
      virtualState.colorAttachments.back().loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    newPassData.reads.emplace_back(attachment.resource);
    newPassData.writes.emplace_back(attachment.resource);

    newPassData.accesses.emplace(
        attachment.resource,
        std::make_pair(attachment.access, attachment.pipelines));
  }

  virtualState.hasDepthStencilAttachment =
      RenderState::TopOfStack->hasDepthStencilAttachment;
  if (RenderState::TopOfStack->hasDepthStencilAttachment) {
    virtualState.depthStencilAttachment =
        RenderState::TopOfStack->depthStencilAttachment;

    if (!RenderState::TopOfStack->rendertargetsUpdated) {
      virtualState.depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    newPassData.reads.emplace_back(
        RenderState::TopOfStack->depthStencilAttachment.texture);

    newPassData.writes.emplace_back(
        RenderState::TopOfStack->depthStencilAttachment.texture);

    newPassData.accesses.emplace(virtualState.depthStencilAttachment.texture,
                                 std::make_pair(depthAccess, depthPipelines));
  }

  RenderState::TopOfStack->rendertargetsUpdated = false;
}

auto VirtualCommandBuffer::GetNewCommand(CommandType type) -> CommandID {
  ZoneScoped;

  if (caches.size() <= (uint8_t)type) {
    caches.resize((uint8_t)type + 1);
  }

  auto &cache = caches[(uint8_t)type];
  CommandID command = InvalidCommandID;

  if (cache.empty()) {
    command = commandStorage.size();
    commandStorage.emplace_back(GetNewEmptyArgs(type));
    commandStorage.back().id = command;
  } else {
    command = cache.back();
    cache.pop_back();
    auto &cmd = commandStorage.at(command);
    cmd.id = command;
    cmd.parentID = UINT16_MAX;

    auto *bound = get_if_derived<BoundResources>(cmd.data);
    if (bound != nullptr) {
      bound->reads.clear();
      bound->writes.clear();
      bound->accesses.clear();
    }
  }

  if (GetRequiresRendering(type)) {
    RenderPass *renderPass = nullptr;

    if (currentRenderPass == InvalidCommandID) {
      NewRenderpass();
      renderPass =
          &std::get<RenderPass>(commandStorage.at(currentRenderPass).data);
    } else {
      renderPass =
          &std::get<RenderPass>(commandStorage.at(currentRenderPass).data);

      if (renderPass->state != *RenderState::TopOfStack) {
        NewRenderpass();

        renderPass =
            &std::get<RenderPass>(commandStorage.at(currentRenderPass).data);
      }
    }

    renderPass->commands.emplace_back(command);
    commandStorage.at(command).parentID = currentRenderPass;
  } else {
    currentRenderPass = InvalidCommandID;
    commands.emplace_back(command);
  }

  return command;
}

auto VirtualCommandBuffer::AddCommand(CommandID command) -> Error {
  ZoneScoped;

  Command *cmd = &GetCommand(command);

  auto *state = get_if_derived<DrawState>(cmd->data);
  if (state != nullptr) {
    CHECK_ERR(state->Initialize(cmd->GetType()));

    // cmd could have been moved after Initialize.
    cmd = &GetCommand(command);
  }

  BoundResources *boundState = nullptr;
  if (cmd->parentID == UINT16_MAX) {
    boundState = get_if_derived<BoundResources>(cmd->data);
  } else {
    boundState = get_if_derived<BoundResources>(GetCommand(cmd->parentID).data);
  }

  if (boundState != nullptr) {
    ZoneScopedN("map read & writes");

    GetReadsInternal(*cmd, state, boundState->reads);
    GetWritesInternal(*cmd, state, boundState->writes);

    assert(threadInfo.parent == this);
    threadInfo.AddCommand(command);
  }

  return {};
}

auto VirtualCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
                                uint32_t firstVertex, uint32_t firstInstance)
    -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdDraw);
  auto &commandInfo = std::get<Args::VkCmdDraw>(GetCommand(command).data);

  commandInfo.vertexCount = vertexCount;
  commandInfo.instanceCount = instanceCount;
  commandInfo.firstVertex = firstVertex;
  commandInfo.firstInstance = firstInstance;

  return AddCommand(command);
}

auto VirtualCommandBuffer::DrawIndexed(uint32_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t firstInstance) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdDrawIndexed);
  auto &commandInfo =
      std::get<Args::VkCmdDrawIndexed>(GetCommand(command).data);

  commandInfo.indexCount = indexCount;
  commandInfo.instanceCount = instanceCount;
  commandInfo.firstIndex = firstIndex;
  commandInfo.vertexOffset = vertexOffset;
  commandInfo.firstInstance = firstInstance;

  return AddCommand(command);
}

auto VirtualCommandBuffer::DrawIndirect(VkBuffer buffer, VkDeviceSize offset,
                                        uint32_t drawCount, uint32_t stride)
    -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdDrawIndirect);
  auto &commandInfo =
      std::get<Args::VkCmdDrawIndirect>(GetCommand(command).data);

  commandInfo.buffer = buffer;
  commandInfo.offset = offset;
  commandInfo.drawCount = drawCount;
  commandInfo.stride = stride;

  return AddCommand(command);
}

auto VirtualCommandBuffer::DrawIndexedIndirect(VkBuffer buffer,
                                               VkDeviceSize offset,
                                               uint32_t drawCount,
                                               uint32_t stride) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdDrawIndexedIndirect);
  auto &commandInfo =
      std::get<Args::VkCmdDrawIndexedIndirect>(GetCommand(command).data);

  commandInfo.buffer = buffer;
  commandInfo.offset = offset;
  commandInfo.drawCount = drawCount;
  commandInfo.stride = stride;

  return AddCommand(command);
}

auto VirtualCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                    uint32_t groupCountZ) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdDispatch);
  auto &commandInfo = std::get<Args::VkCmdDispatch>(GetCommand(command).data);

  commandInfo.groupCountX = groupCountX;
  commandInfo.groupCountY = groupCountY;
  commandInfo.groupCountZ = groupCountZ;

  return AddCommand(command);
}

auto VirtualCommandBuffer::DispatchIndirect(VkBuffer buffer,
                                            VkDeviceSize offsets) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdDispatchIndirect);
  auto &commandInfo =
      std::get<Args::VkCmdDispatchIndirect>(GetCommand(command).data);

  commandInfo.buffer = buffer;
  commandInfo.offset = offsets;

  return AddCommand(command);
}

auto VirtualCommandBuffer::BlitImage(
    VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
    VkImageLayout dstImageLayout, uint32_t regionCount,
    const VkImageBlit *pRegions, VkFilter filter) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdBlitImage);
  auto &commandInfo = std::get<Args::VkCmdBlitImage>(GetCommand(command).data);

  commandInfo.srcImage = srcImage;
  commandInfo.srcImageLayout = srcImageLayout;
  commandInfo.dstImage = dstImage;
  commandInfo.dstImageLayout = dstImageLayout;
  commandInfo.regions.resize(regionCount);
  memcpy(commandInfo.regions.data(), pRegions,
         sizeof(VkImageBlit) * regionCount);
  commandInfo.filter = filter;

  return AddCommand(command);
}

auto VirtualCommandBuffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer,
                                      uint32_t regionCount,
                                      const VkBufferCopy *pRegions) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdCopyBuffer);
  auto &commandInfo = std::get<Args::VkCmdCopyBuffer>(GetCommand(command).data);

  commandInfo.srcBuffer = srcBuffer;
  commandInfo.dstBuffer = dstBuffer;
  commandInfo.regions.resize(regionCount);
  memcpy(commandInfo.regions.data(), pRegions,
         sizeof(VkBufferCopy) * regionCount);

  return AddCommand(command);
}

auto VirtualCommandBuffer::CopyImage(VkImage srcImage,
                                     VkImageLayout srcImageLayout,
                                     VkImage dstImage,
                                     VkImageLayout dstImageLayout,
                                     uint32_t regionCount,
                                     const VkImageCopy *pRegions) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdCopyImage);
  auto &commandInfo = std::get<Args::VkCmdCopyImage>(GetCommand(command).data);

  commandInfo.srcImage = srcImage;
  commandInfo.srcImageLayout = srcImageLayout;
  commandInfo.dstImage = dstImage;
  commandInfo.dstImageLayout = dstImageLayout;

  commandInfo.regions.resize(regionCount);
  memcpy(commandInfo.regions.data(), pRegions,
         sizeof(VkImageCopy) * regionCount);

  commandInfo.srcResources.reserve(regionCount);
  commandInfo.srcResources.clear();

  commandInfo.dstResources.reserve(regionCount);
  commandInfo.dstResources.clear();

  for (auto &region : commandInfo.regions) {
    commandInfo.srcResources.emplace_back(ImageSubresource(
        srcImage, region.srcSubresource.baseArrayLayer,
        region.srcSubresource.layerCount, region.srcSubresource.mipLevel, 1));

    commandInfo.dstResources.emplace_back(ImageSubresource(
        dstImage, region.dstSubresource.baseArrayLayer,
        region.dstSubresource.layerCount, region.dstSubresource.mipLevel, 1));
  }

  return AddCommand(command);
}

auto VirtualCommandBuffer::CopyBufferToImage(
    VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout,
    uint32_t regionCount, const VkBufferImageCopy *pRegions) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdCopyBufferToImage);
  auto &commandInfo =
      std::get<Args::VkCmdCopyBufferToImage>(GetCommand(command).data);

  commandInfo.srcBuffer = srcBuffer;
  commandInfo.dstImage = dstImage;
  commandInfo.dstImageLayout = dstImageLayout;

  commandInfo.regions.resize(regionCount);
  memcpy(commandInfo.regions.data(), pRegions,
         sizeof(VkBufferImageCopy) * regionCount);

  commandInfo.dstResources.reserve(regionCount);
  commandInfo.dstResources.clear();

  for (auto &region : commandInfo.regions) {
    commandInfo.dstResources.emplace_back(
        ImageSubresource(dstImage, region.imageSubresource.baseArrayLayer,
                         region.imageSubresource.layerCount,
                         region.imageSubresource.mipLevel, 1));
  }

  return AddCommand(command);
}

auto VirtualCommandBuffer::CopyImageToBuffer(
    VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer,
    uint32_t regionCount, const VkBufferImageCopy *pRegions) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdCopyImageToBuffer);
  auto &commandInfo =
      std::get<Args::VkCmdCopyImageToBuffer>(GetCommand(command).data);

  commandInfo.srcImage = srcImage;
  commandInfo.srcImageLayout = srcImageLayout;
  commandInfo.dstBuffer = dstBuffer;

  commandInfo.regions.resize(regionCount);
  memcpy(commandInfo.regions.data(), pRegions,
         sizeof(VkBufferImageCopy) * regionCount);

  commandInfo.srcResources.reserve(regionCount);
  commandInfo.srcResources.clear();

  for (auto &region : commandInfo.regions) {
    commandInfo.srcResources.emplace_back(
        ImageSubresource(srcImage, region.imageSubresource.baseArrayLayer,
                         region.imageSubresource.layerCount,
                         region.imageSubresource.mipLevel, 1));
  }

  return AddCommand(command);
}

auto VirtualCommandBuffer::FillBuffer(VkBuffer dstBuffer,
                                      VkDeviceSize dstOffset, VkDeviceSize size,
                                      uint32_t data) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdFillBuffer);
  auto &commandInfo = std::get<Args::VkCmdFillBuffer>(GetCommand(command).data);

  commandInfo.dstBuffer = dstBuffer;
  commandInfo.dstOffset = dstOffset;
  commandInfo.size = size;
  commandInfo.data = data;

  return AddCommand(command);
}

auto VirtualCommandBuffer::BuildAccelerationStructuresKHR(
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos,
    uint32_t readCount, VkBuffer const *bufferReads, uint32_t writeCount,
    VkBuffer const *bufferWrites) -> Error {
  CommandID command =
      GetNewCommand(CommandType::vkCmdBuildAccelerationStructuresKHR);
  auto &commandInfo = std::get<Args::VkCmdBuildAccelerationStructuresKHR>(
      GetCommand(command).data);

  commandInfo.infoCount = infoCount;
  commandInfo.infos.resize(infoCount);
  commandInfo.buildRangeInfos.resize(infoCount);
  commandInfo.bufferReads.resize(readCount);
  commandInfo.bufferWrites.resize(writeCount);

  assert(readCount != 0U);
  assert(writeCount != 0U);
  assert(infoCount != 0U);

  memcpy(commandInfo.infos.data(), pInfos,
         sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * infoCount);

  memcpy(commandInfo.bufferReads.data(), bufferReads, // NOLINT
         sizeof(VkBuffer) * readCount);

  memcpy(commandInfo.bufferWrites.data(), bufferWrites, // NOLINT
         sizeof(VkBuffer) * writeCount);

  size_t geometryCount{};

  for (const auto &info : commandInfo.infos) {
    geometryCount += info.geometryCount;
  }

  commandInfo.geometries.reserve(geometryCount);
  commandInfo.geometries.clear();

  for (size_t i = 0; i < infoCount; ++i) {
    assert(commandInfo.infos[i].pNext == nullptr);
    assert(commandInfo.infos[i].ppGeometries == nullptr);

    const size_t offset = commandInfo.geometries.size();

    for (uint32_t j = 0; j < commandInfo.infos[i].geometryCount; ++j) {
      commandInfo.geometries.emplace_back(
          commandInfo.infos[i].pGeometries[j]); // NOLINT
    }

    commandInfo.buildRangeInfos[i] = *ppBuildRangeInfos[i]; // NOLINT
  }

  return AddCommand(command);
}

auto VirtualCommandBuffer::CopyAccelerationStructureKHR(
    const VkCopyAccelerationStructureInfoKHR *pInfo) -> Error {
  CommandID command =
      GetNewCommand(CommandType::vkCmdCopyAccelerationStructureKHR);
  auto &commandInfo = std::get<Args::VkCmdCopyAccelerationStructureKHR>(
      GetCommand(command).data);

  commandInfo.structureInfo = *pInfo;

  return AddCommand(command);
}

auto VirtualCommandBuffer::PipelineBarrier2(
    const VkDependencyInfo *pDependencyInfo) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdPipelineBarrier2);
  auto &commandInfo =
      std::get<Args::VkCmdPipelineBarrier2>(GetCommand(command).data);

  commandInfo.memoryBarriers.clear();
  commandInfo.bufferMemoryBarriers.clear();
  commandInfo.imageMemoryBarriers.clear();

  if (pDependencyInfo->pMemoryBarriers != nullptr) {
    commandInfo.memoryBarriers.assign(
        pDependencyInfo->pMemoryBarriers,
        pDependencyInfo->pMemoryBarriers + // NOLINT
            pDependencyInfo->memoryBarrierCount);
  }

  if (pDependencyInfo->pBufferMemoryBarriers != nullptr) {
    commandInfo.bufferMemoryBarriers.assign(
        pDependencyInfo->pBufferMemoryBarriers,
        pDependencyInfo->pBufferMemoryBarriers + // NOLINT
            pDependencyInfo->bufferMemoryBarrierCount);
  }

  if (pDependencyInfo->pImageMemoryBarriers != nullptr) {
    commandInfo.imageMemoryBarriers.assign(
        pDependencyInfo->pImageMemoryBarriers,
        pDependencyInfo->pImageMemoryBarriers + // NOLINT
            pDependencyInfo->imageMemoryBarrierCount);
  }

  return AddCommand(command);
}

auto VirtualCommandBuffer::MipmapTexture(Texture *texture) -> Error {
  CommandID command = GetNewCommand(CommandType::mipmapTexture);
  auto &commandInfo = std::get<Args::MipmapTexture>(GetCommand(command).data);

  commandInfo.texture = texture;

  return AddCommand(command);
}

auto VirtualCommandBuffer::ResetQueryPool(VkQueryPool queryPool,
                                          uint32_t firstQuery,
                                          uint32_t queryCount) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdResetQueryPool);
  auto &commandInfo =
      std::get<Args::VkCmdResetQueryPool>(GetCommand(command).data);

  commandInfo.queryPool = queryPool;
  commandInfo.firstQuery = firstQuery;
  commandInfo.queryCount = queryCount;

  return AddCommand(command);
}

auto VirtualCommandBuffer::WriteAccelerationStructuresPropertiesKHR(
    uint32_t accelerationStructureCount,
    const VkAccelerationStructureKHR *pAccelerationStructures,
    VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
    -> Error {
  CommandID command =
      GetNewCommand(CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR);
  auto &commandInfo =
      std::get<Args::VkCmdWriteAccelerationStructuresPropertiesKHR>(
          GetCommand(command).data);

  commandInfo.accelerationStructures.resize(accelerationStructureCount);
  memcpy(commandInfo.accelerationStructures.data(), // NOLINT
         pAccelerationStructures,                   // NOLINT
         sizeof(VkAccelerationStructureKHR) * accelerationStructureCount);
  commandInfo.queryType = queryType;
  commandInfo.queryPool = queryPool;
  commandInfo.firstQuery = firstQuery;

  return AddCommand(command);
}

auto VirtualCommandBuffer::BindIndexBuffer(
    const Args::VkCmdBindIndexBuffer &arguments) -> void {
  currentState.indexBuffer = arguments.buffer;
  currentState.indexType = arguments.indexType;
  currentState.indexBufferOffset = arguments.offset;
}

auto VirtualCommandBuffer::BindVertexBuffers(uint32_t firstBinding,
                                             uint32_t bindingCount,
                                             const VkBuffer *pBuffers,
                                             const VkDeviceSize *pOffsets)
    -> void {
  currentState.vertexBuffers.resize(firstBinding + bindingCount);
  currentState.vertexBufferOffsets.resize(firstBinding + bindingCount);

  for (size_t i = 0; i < bindingCount; ++i) {
    currentState.vertexBuffers[firstBinding + i] = pBuffers[i];       // NOLINT
    currentState.vertexBufferOffsets[firstBinding + i] = pOffsets[i]; // NOLINT
  }
}

auto VirtualCommandBuffer::SetVertexInputEXT(
    uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
    -> void {

  if (vertexBindingDescriptionCount == 0 ||
      vertexAttributeDescriptionCount == 0) {
    currentState.bindingDescriptions.clear();
    currentState.attributeDescriptions.clear();
    return;
  }

  currentState.bindingDescriptions.resize(vertexBindingDescriptionCount);
  currentState.attributeDescriptions.resize(vertexAttributeDescriptionCount);

  memcpy(currentState.bindingDescriptions.data(), pVertexBindingDescriptions,
         sizeof(VkVertexInputBindingDescription2EXT) *
             vertexBindingDescriptionCount);

  memcpy(currentState.attributeDescriptions.data(),
         pVertexAttributeDescriptions,
         sizeof(VkVertexInputAttributeDescription2EXT) *
             vertexAttributeDescriptionCount);
}

auto VirtualCommandBuffer::ClearAttachments(
    uint32_t attachmentCount, const VkClearAttachment *pAttachments,
    uint32_t rectCount, const VkClearRect *pRects) -> Error {
  CommandID command = GetNewCommand(CommandType::vkCmdClearAttachments);
  auto &commandInfo =
      std::get<Args::VkCmdClearAttachments>(GetCommand(command).data);

  commandInfo.attachments.resize(attachmentCount);
  memcpy(commandInfo.attachments.data(), pAttachments,
         sizeof(VkClearAttachment) * attachmentCount);
  commandInfo.rects.resize(rectCount);
  memcpy(commandInfo.rects.data(), pRects, sizeof(VkClearRect) * rectCount);

  return AddCommand(command);
}

auto VirtualCommandBuffer::BeginDebugUtilsLabelEXT(
    const Args::VkCmdBeginDebugUtilsLabelEXT &arguments) -> void {
  // currentState.currentDebugMarker = arguments.labelInfo.pLabelName;
}

auto VirtualCommandBuffer::EndDebugUtilsLabelEXT(
    const Args::VkCmdEndDebugUtilsLabelEXT &arguments) -> void {}

auto VirtualCommandBuffer::InsertDebugUtilsLabelEXT(
    const Args::VkCmdInsertDebugUtilsLabelEXT &arguments) -> void {}

auto GetRequiresRendering(CommandType type) -> bool {
  switch (type) {
  case CommandType::vkCmdDraw:
  case CommandType::vkCmdDrawIndexed:
  case CommandType::vkCmdDrawIndirect:
  case CommandType::vkCmdDrawIndexedIndirect:
  case CommandType::vkCmdDispatch:
  case CommandType::vkCmdDispatchIndirect:
  case CommandType::vkCmdClearAttachments:
    return true;
  case CommandType::vkCmdBlitImage:
  case CommandType::vkCmdCopyBuffer:
  case CommandType::vkCmdCopyImage:
  case CommandType::vkCmdCopyBufferToImage:
  case CommandType::vkCmdCopyImageToBuffer:
  case CommandType::mipmapTexture:
  case CommandType::vkCmdFillBuffer:
  case CommandType::vkCmdBuildAccelerationStructuresKHR:
  case CommandType::vkCmdCopyAccelerationStructureKHR:
  case CommandType::vkCmdResetQueryPool:
  case CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR:
  case CommandType::vkCmdPipelineBarrier2:
  case CommandType::renderPass:
    return false;
  }
}

auto CreateCommandBuffer() -> VirtualCommandBuffer { return {}; }

auto VirtualCommandBuffer::Reset() -> void {
  ZoneScoped;

  for (const auto commandID : commands) {
    const auto &command = GetCommand(commandID);
    if (command.type == CommandType::renderPass) {
      for (const auto childID : std::get<RenderPass>(command.data).commands) {
        caches.at((uint8_t)GetCommand(childID).type).emplace_back(childID);
      }
    }

    caches.at((uint8_t)command.type).emplace_back(commandID);
  }

  commands.clear();
  currentRenderPass = InvalidCommandID;
  queueFamily = UINT32_MAX;
  currentState = {};
  currentState.dirty = true;
}

auto DrawState::Apply(const GraphicsContext &context, const RenderPass &parent,
                      VkCommandBuffer cmdBuffer) const -> Error {
  ZoneScoped;

  const VirtualRenderState &state = parent.state;

  RecordingState::CurrentState.renderState = &parent.state;
  assert(RecordingState::CurrentState.renderState->shader.get() != nullptr);

  if (state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    ZoneScopedN("Update state");

    RecordingState::CurrentState.cullMode = cullMode;
    RecordingState::CurrentState.frontFace = frontFace;
    RecordingState::CurrentState.depthTestEnable = depthTestEnable;
    RecordingState::CurrentState.depthWriteEnable = depthWriteEnable;
    RecordingState::CurrentState.depthCompareOp = depthCompareOp;
    RecordingState::CurrentState.viewport = viewport;
    RecordingState::CurrentState.scissor = scissor;

    vkCmdSetVertexInputEXT(
        cmdBuffer, bindingDescriptions.size(), bindingDescriptions.data(),
        attributeDescriptions.size(), attributeDescriptions.data());

    if (!vertexBuffers.empty()) {
      vkCmdBindVertexBuffers(cmdBuffer, 0, vertexBuffers.size(),
                             vertexBuffers.data(), vertexBufferOffsets.data());
    }

    if (indexBuffer != VK_NULL_HANDLE) {
      vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexBufferOffset,
                           indexType);
    }
  } else {
    ERR_ASSERT(state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE);
  }

  VkPushDataInfoEXT pushInfo{
      .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
      .pNext = nullptr,
      .offset = 0,
      .data{
          .address = &uboAddress,
          .size = sizeof(uint32_t),
      },
  };

  CHECK_ERR(PrepareRendering(context, cmdBuffer));

  vkCmdPushDataEXT(cmdBuffer, &pushInfo);

  return {};
}

} // namespace Graphics