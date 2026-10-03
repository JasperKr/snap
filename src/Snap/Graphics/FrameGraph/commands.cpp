#include "commands.hpp"
#include "Graphics/FrameGraph/descriptorState.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/vkAccessHelpers.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "dynamicRendering.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <public/tracy/Tracy.hpp>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

std::unordered_map<GraphState, uint32_t, GraphStateHash>
    CommandStateManager::StateToIndex{};
std::vector<GraphState> CommandStateManager::States{};
uint32_t CommandStateManager::CurrentStateID = UINT32_MAX;

ImageSubresource::ImageSubresource(const Ref<Texture> &texture)
    : layerStart(static_cast<uint16_t>(texture->baseArrayLayer)),
      layerCount(static_cast<uint16_t>(texture->layerCount)),
      mipStart(static_cast<uint16_t>(texture->baseMipLevel)),
      mipCount(static_cast<uint16_t>(texture->levelCount)),
      image(texture->imageMemory->image) {}

ImageSubresource::ImageSubresource(const Texture *texture)
    : layerStart(static_cast<uint16_t>(texture->baseArrayLayer)),
      layerCount(static_cast<uint16_t>(texture->layerCount)),
      mipStart(static_cast<uint16_t>(texture->baseMipLevel)),
      mipCount(static_cast<uint16_t>(texture->levelCount)),
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

auto DrawState::GetAccesses(
    CommandType type,
    std::unordered_map<VulkanResource,
                       std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                       VulkanResourceHash<true>> &accesses) const -> void {

  static const auto getFlags =
      [](const std::vector<BoundResource> &boundResources,
         std::unordered_map<VulkanResource,
                            std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                            VulkanResourceHash<true>> &accesses) -> void {
    for (const auto &bound : boundResources) {
      accesses.emplace(bound.resource,
                       std::make_pair(bound.access, bound.pipelines));
    }
  };

  getFlags(boundImages, accesses);
  getFlags(boundBuffers, accesses);
  getFlags(boundASs, accesses);

  const auto &graphState = GetGraphState();

  if (graphState.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return;
  }

  for (int i = 0; i < colorAttachments.size(); i++) {
    const auto &attachment = colorAttachments.at(i);

    const bool blendEnabled =
        graphState.colorAttachments.at(i).blendMode.blendEnable != 0U;

    VkAccessFlags2 access{};
    VkPipelineStageFlags2 pipelines{};

    if (blendEnabled) {
      access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
    }

    access |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    pipelines |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    accesses.emplace(attachment.resource, std::make_pair(access, pipelines));
  }

  if (depthStencilAttachment.has_value()) {
    VkAccessFlags2 access{};
    VkPipelineStageFlags2 pipelines{};

    if (graphState.depthTestEnable != 0U) {
      access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      pipelines |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    }

    if (graphState.depthWriteEnable != 0U) {
      access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      pipelines |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    }

    accesses.emplace(depthStencilAttachment->resource,
                     std::make_pair(access, pipelines));
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

auto DrawState::GetReadStateFor(const VulkanResource &resource,
                                CommandType type) const
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  SyncFlags flags{};

  static const auto getFlags =
      [](const std::vector<BoundResource> &boundResources,
         const VulkanResource &resource) -> SyncFlags {
    VkAccessFlags2 access = 0;
    VkPipelineStageFlags2 pipelines = 0;

    for (const auto &bound : boundResources) {
      if (bound.Overlaps(resource) &&
          VkAccessHelpers::IsReadAccess(bound.access)) {
        access |= bound.access;
        pipelines |= bound.pipelines;
      }
    }

    return {.access = access, .pipelines = pipelines};
  };

  flags |= getFlags(boundImages, resource);
  flags |= getFlags(boundBuffers, resource);
  flags |= getFlags(boundASs, resource);

  const auto &graphState = GetGraphState();

  if (graphState.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return flags.Get();
  }

  if (resource.type == VulkanResource::ResourceType::Image) {
    if (depthStencilAttachment.has_value() &&
        depthStencilAttachment->Overlaps(resource) &&
        graphState.depthWriteEnable == 0U && graphState.depthTestEnable == 1U) {
      flags.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
      flags.pipelines |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    }
  } else if (resource.type == VulkanResource::ResourceType::Buffer) {
    for (const auto &buffer : vertexBuffers) {
      if (buffer == resource.buffer) {
        flags.access |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
        flags.pipelines |= VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT;
      }
    }

    if (indexBuffer == resource.buffer) {
      flags.access |= VK_ACCESS_2_INDEX_READ_BIT;
      flags.pipelines |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    }
  }

  return flags.Get();
}

auto DrawState::GetWriteStateFor(const VulkanResource &resource,
                                 CommandType type) const
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  SyncFlags flags{};

  static const auto getFlags =
      [](const std::vector<BoundResource> &boundResources,
         const VulkanResource &resource) -> SyncFlags {
    VkAccessFlags2 access = 0;
    VkPipelineStageFlags2 pipelines = 0;

    for (const auto &bound : boundResources) {
      if (bound.Overlaps(resource) &&
          VkAccessHelpers::IsWriteAccess(bound.access)) {
        access |= bound.access;
        pipelines |= bound.pipelines;
      }
    }

    return {.access = access, .pipelines = pipelines};
  };

  flags |= getFlags(boundImages, resource);
  flags |= getFlags(boundBuffers, resource);
  flags |= getFlags(boundASs, resource);

  const auto &graphState = GetGraphState();

  if (graphState.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return flags.Get();
  }

  if (resource.type == VulkanResource::ResourceType::Buffer) {
    for (const auto &attachment : colorAttachments) {
      if (attachment.Overlaps(resource)) {
        flags.access |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        flags.pipelines |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      }
    }

    if (depthStencilAttachment.has_value() &&
        depthStencilAttachment->Overlaps(resource)) {
      if (graphState.depthWriteEnable != 0U) {
        flags.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        flags.pipelines |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      }
    }
  }

  return flags.Get();
}

auto GraphState::GetHash() const -> uint64_t {
  if (dirty) {
    Hash::Hasher hasher{};

    // Special case for compute pipelines
    if (bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      hasher.Add(std::hash<VkPipelineBindPoint>()(bindPoint));
      hasher.Add(shader->getID());
      return hasher.Get();
    }

    hasher.Add(cullMode);
    hasher.Add(frontFace);
    hasher.Add(depthTestEnable);
    hasher.Add(depthWriteEnable);
    hasher.Add(depthCompareOp);
    hasher.Add(stencilTestEnable);
    hasher.Add(polygonMode);
    hasher.Add(viewport.x);
    hasher.Add(viewport.y);
    hasher.Add(viewport.width);
    hasher.Add(viewport.height);
    hasher.Add(scissor.extent.width);
    hasher.Add(scissor.extent.height);
    hasher.Add(scissor.offset.x);
    hasher.Add(scissor.offset.y);

    for (const auto &equation : colorBlendEquations) {
      hasher.Add(equation.srcColorBlendFactor);
      hasher.Add(equation.dstColorBlendFactor);
      hasher.Add(equation.colorBlendOp);
      hasher.Add(equation.srcAlphaBlendFactor);
      hasher.Add(equation.dstAlphaBlendFactor);
      hasher.Add(equation.alphaBlendOp);
    }

    if (shader) {
      hasher.Add(shader->getID());
    }

    hasher.Add(primitiveTopology);
    hasher.Add(bindPoint);

    for (const auto &attachment : colorAttachments) {
      hasher.Add(attachment.texture->view);
    }

    if (hasDepthStencilAttachment) {
      hasher.Add(depthStencilAttachment.texture->view);
    }

    for (const auto &desc : bindingDescriptions) {
      hasher.Add(desc.binding);
      hasher.Add(desc.stride);
      hasher.Add(desc.inputRate);
    }

    for (const auto &desc : attributeDescriptions) {
      hasher.Add(desc.location);
      hasher.Add(desc.binding);
      hasher.Add(desc.format);
      hasher.Add(desc.offset);
    }

    hash = hasher.Get();

    dirty = false;
  }

  return hash;
}

// NOLINTNEXTLINE
auto BindDefaultTextures(const GraphicsContext &context, Shader *shader)
    -> Error {
  ZoneScoped;
  auto &state = shader->GetState();

  for (const auto &resource : shader->reflection.resources) {
    if (resource.IsSampler()) {
      const auto &samplerInfo = std::get<Reflect::SamplerInfo>(resource.info);
      auto key = Utils::SetBindingToSlot(samplerInfo.set, samplerInfo.binding);
      if (state.userBoundTextures.contains(key)) {
        continue;
      }

      VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
      TextureType type = TextureType::ENUM_MAX;

      if ((samplerInfo.shape & SLANG_TEXTURE_3D) == SLANG_TEXTURE_3D) {
        if ((samplerInfo.shape & SLANG_TEXTURE_ARRAY_FLAG) ==
            SLANG_TEXTURE_ARRAY_FLAG) {
          return Error::Create("3D texture arrays are not supported.");
        }
        type = TextureType::VOLUME;
      } else if ((samplerInfo.shape & SLANG_TEXTURE_CUBE) ==
                 SLANG_TEXTURE_CUBE) {
        if ((samplerInfo.shape & SLANG_TEXTURE_ARRAY_FLAG) ==
            SLANG_TEXTURE_ARRAY_FLAG) {
          return Error::Create("Cubemap texture arrays are not supported.");
        }
        type = TextureType::CUBEMAP;
      } else if ((samplerInfo.shape & SLANG_TEXTURE_2D) == SLANG_TEXTURE_2D) {
        if ((samplerInfo.shape & SLANG_TEXTURE_ARRAY_FLAG) ==
            SLANG_TEXTURE_ARRAY_FLAG) {
          type = TextureType::ARRAY;
        } else {
          type = TextureType::DEFAULT;
        }
      }

      auto defaultTexture =
          CHECK_RES(Texture::GetDefault(context, format, type));
      state.userBoundTextures[key] = {defaultTexture, &samplerInfo};
    }
  }

  return Error::Success();
}

auto DrawState::Initialize(CommandType type) -> Error {
  ZoneScoped;

  const auto &shader = RenderState::GetShader();
  const auto &context = *GetCurrentGraphicsContext();

  const auto &ctx = GetThreadContext();
  auto &graphState = ctx.commandBuffer->GetGraphState();

  if (graphState.shader != shader ||
      graphState.bindPoint != RenderState::GetBindPoint()) {
    graphState.MarkUpdated();
  }

  graphState.shader = shader;
  graphState.bindPoint = RenderState::GetBindPoint();

  if (shader->pushBuffer) {
    const auto &data = shader->pushBuffer->GetData();
    pushConstants.resize(data.size());
    memcpy(pushConstants.data(), data.data(), data.size());
  }

  bool isCompute =
      (shader->combinedShaderStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0;

  if (!isCompute || type == CommandType::vkCmdClearAttachments) {
    graphState.scissor = RenderState::GetScissor();
    graphState.viewport = RenderState::GetClippedViewport();
    graphState.cullMode = RenderState::GetCullMode();
    graphState.polygonMode = RenderState::GetPolygonMode();
    graphState.depthTestEnable = RenderState::TopOfStack->depthTestEnable;
    graphState.depthWriteEnable = RenderState::TopOfStack->depthWriteEnable;
    graphState.depthCompareOp = RenderState::TopOfStack->depthCompareOp;
    graphState.stencilTestEnable = RenderState::TopOfStack->stencilTestEnable;
    graphState.frontFace = RenderState::GetWindingOrder();
    graphState.primitiveTopology = RenderState::GetTopology();
    graphState.colorBlendEquations =
        RenderState::TopOfStack->colorBlendEquations;
    graphState.colorAttachments = RenderState::TopOfStack->colorAttachments;
    graphState.depthStencilAttachment =
        RenderState::TopOfStack->depthStencilAttachment;
    graphState.hasDepthStencilAttachment =
        RenderState::TopOfStack->hasDepthStencilAttachment;
    graphState.MarkUpdated();
  }

  if (!isCompute) {
    VkAccessFlags2 depthAccess = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 depthPipelines = VK_PIPELINE_STAGE_2_NONE;

    // clang-format off
    depthAccess |= (graphState.depthTestEnable != 0U) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT : 0U;
    depthAccess |= (graphState.depthWriteEnable != 0U) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0U;

    depthPipelines |= (graphState.depthTestEnable != 0U) ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT : 0U;
    depthPipelines |= (graphState.depthWriteEnable != 0U) ? VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT : 0U;
    // clang-format on

    colorAttachments.reserve(RenderState::TopOfStack->colorAttachments.size());
    colorAttachments.clear();

    for (const auto &target : RenderState::TopOfStack->colorAttachments) {
      colorAttachments.emplace_back(BoundResource{
          .resource = ImageSubresource{target.texture},
          // clang-format off
            .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | ((target.blendMode.blendEnable != 0U) ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT : 0U),
            .pipelines = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          // clang-format on
      });
    }

    if (RenderState::TopOfStack->hasDepthStencilAttachment) {
      depthStencilAttachment = BoundResource{
          .resource =
              ImageSubresource{
                  RenderState::TopOfStack->depthStencilAttachment.texture},
          .access = depthAccess,
          .pipelines = depthPipelines};
    }
  }

  const auto &shaderState = shader->GetState();
  const auto &pipelines = shader->combinedPipelineStages;

  boundImages.reserve(shaderState.userBoundTextures.size());
  boundBuffers.reserve(shaderState.userBoundBuffers.size());
  boundASs.reserve(shaderState.userBoundAccelerationStructures.size());

  boundImages.clear();
  boundBuffers.clear();
  boundASs.clear();

  for (const auto &texture : shaderState.userBoundTextures) {
    boundImages.emplace_back(BoundResource{
        .resource = ImageSubresource{texture.second.first},
        .access = texture.second.second->accessFlags,
        .pipelines = pipelines,
    });
  }

  for (const auto &buffer : shaderState.userBoundBuffers) {
    boundBuffers.emplace_back(BoundResource{
        .resource = buffer.second.first->handle,
        .access = buffer.second.second->accessFlags,
        .pipelines = pipelines,
    });
  }

  for (const auto &accelerationStructure :
       shaderState.userBoundAccelerationStructures) {
    boundASs.emplace_back(BoundResource{
        .resource =
            accelerationStructure.second.first->GetAccelerationStructure(),
        .access = accelerationStructure.second.second->access,
        .pipelines = pipelines,
    });
  }

  vertexBuffers.clear();
  vertexBuffers.insert(vertexBuffers.begin(), graphState.vertexBuffers.begin(),
                       graphState.vertexBuffers.end());

  vertexBufferOffsets.clear();
  vertexBufferOffsets.insert(vertexBufferOffsets.begin(),
                             graphState.vertexBufferOffsets.begin(),
                             graphState.vertexBufferOffsets.end());

  indexBuffer = graphState.indexBuffer;
  indexBufferOffset = graphState.indexBufferOffset;
  indexType = graphState.indexType;

  thread_local uint32_t lastID{};
  thread_local GraphState lastState;

  if (ctx.commandBuffer->currentState == lastState) {
    stateID = lastID;
  } else {
    stateID = ctx.commandBuffer->GetStateID();
    lastID = stateID;
    lastState = ctx.commandBuffer->currentState;
  }

  CHECK_ERR(BindDefaultTextures(context, shader.get()));

  std::tie(descriptorSets, dynamicOffsets) =
      CHECK_RES(GetDescriptorSets(*GetCurrentGraphicsContext()));

  return {};
}

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

inline auto GetReadsFromDrawState(DrawState &state, bool getRendertargets,
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

  if (!getRendertargets) {
    return;
  }

  for (const auto &vertexBuffer : state.vertexBuffers) {
    if (vertexBuffer != VK_NULL_HANDLE) {
      reads.emplace_back(vertexBuffer);
    }
  }

  if (state.indexBuffer != VK_NULL_HANDLE) {
    reads.emplace_back(state.indexBuffer);
  }

  for (const auto &colorAttachment : state.colorAttachments) {
    reads.emplace_back(colorAttachment.resource);
  }

  if (state.depthStencilAttachment.has_value()) {
    reads.emplace_back(state.depthStencilAttachment->resource);
  }
}

inline auto GetReadsInternal(Command &command,
                             std::vector<VulkanResource> &reads) -> void {
  auto *drawState = command.GetDrawState();

  if (drawState != nullptr) {
    bool getRendertargets =
        drawState->GetGraphState().bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

    GetReadsFromDrawState(*drawState, getRendertargets, reads);
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

inline auto GetWritesFromDrawState(DrawState &state, bool getRendertargets,
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

  if (!getRendertargets) {
    return;
  }

  for (const auto &colorAttachment : state.colorAttachments) {
    writes.emplace_back(colorAttachment.resource);
  }

  if (state.depthStencilAttachment.has_value()) {
    writes.emplace_back(state.depthStencilAttachment->resource);
  }
}

inline auto GetWritesInternal(Command &command,
                              std::vector<VulkanResource> &writes) -> void {
  auto *drawState = command.GetDrawState();

  if (drawState != nullptr) {
    bool getRendertargets =
        drawState->GetGraphState().bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;
    getRendertargets = getRendertargets ||
                       command.GetType() == CommandType::vkCmdClearAttachments;

    GetWritesFromDrawState(*drawState, getRendertargets, writes);
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
inline auto GetResourceAccesses(
    const Command &command,
    std::unordered_map<VulkanResource,
                       std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                       VulkanResourceHash<true>> &accesses) -> void {

  const auto *drawState = command.GetDrawState();
  const auto *boundState = get_if_derived<BoundResources>(command.data);

  if (drawState != nullptr) {
    drawState->GetAccesses(command.GetType(), accesses);
    return;
  }

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
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
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    accesses.emplace(VulkanResource(ImageSubresource(args.texture)),
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_TRANSFER_BIT));
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    accesses.emplace(VulkanResource(args.srcBuffer),
                     std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
    accesses.emplace(VulkanResource(args.dstBuffer),
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    for (const auto &resource : args.srcResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    for (const auto &resource : args.dstResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    accesses.emplace(args.srcBuffer,
                     std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
    for (const auto &resource : args.dstResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    for (const auto &resource : args.srcResources) {
      accesses.emplace(resource, std::make_pair(VK_ACCESS_2_TRANSFER_READ_BIT,
                                                VK_PIPELINE_STAGE_2_COPY_BIT));
    }
    accesses.emplace(args.dstBuffer,
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_COPY_BIT));
  }

  if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    accesses.emplace(args.dstBuffer,
                     std::make_pair(VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_2_TRANSFER_BIT));
  }

  if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
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
  }
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
}

auto VirtualCommandBuffer::GetNewCommand(CommandType type) -> Command & {
  ZoneScoped;

  auto &cache = caches.at((uint8_t)type);
  Command *command = nullptr;

  const auto cacheSize = cache.size();

  if (cacheSize == 0) {
    command = &commands.emplace_back(GetNewEmptyArgs(type));
  } else {
    command = &commands.emplace_back(cache.at(cacheSize - 1ULL));
    cache.pop_back();
  }

  return *command;
}

auto VirtualCommandBuffer::AddCommand(Command &command) -> Error {
  ZoneScoped;

  auto *state = command.GetDrawState();
  if (state != nullptr) {
    CHECK_ERR(state->Initialize(command.GetType()));
  }

  auto *boundState = get_if_derived<BoundResources>(command.data);

  if (boundState != nullptr) {
    ZoneScopedN("map read & writes");

    boundState->reads.clear();
    boundState->writes.clear();
    boundState->accesses.clear();

    GetReadsInternal(command, boundState->reads);
    GetWritesInternal(command, boundState->writes);
    GetResourceAccesses(command, boundState->accesses);
  }

  return {};
}

auto VirtualCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
                                uint32_t firstVertex, uint32_t firstInstance)
    -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdDraw);
  auto &commandInfo = std::get<Args::VkCmdDraw>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdDrawIndexed);
  auto &commandInfo = std::get<Args::VkCmdDrawIndexed>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdDrawIndirect);
  auto &commandInfo = std::get<Args::VkCmdDrawIndirect>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdDrawIndexedIndirect);
  auto &commandInfo = std::get<Args::VkCmdDrawIndexedIndirect>(command.data);

  commandInfo.buffer = buffer;
  commandInfo.offset = offset;
  commandInfo.drawCount = drawCount;
  commandInfo.stride = stride;

  return AddCommand(command);
}

auto VirtualCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                    uint32_t groupCountZ) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdDispatch);
  auto &commandInfo = std::get<Args::VkCmdDispatch>(command.data);

  commandInfo.groupCountX = groupCountX;
  commandInfo.groupCountY = groupCountY;
  commandInfo.groupCountZ = groupCountZ;

  return AddCommand(command);
}

auto VirtualCommandBuffer::DispatchIndirect(VkBuffer buffer,
                                            VkDeviceSize offsets) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdDispatchIndirect);
  auto &commandInfo = std::get<Args::VkCmdDispatchIndirect>(command.data);

  commandInfo.buffer = buffer;
  commandInfo.offset = offsets;

  return AddCommand(command);
}

auto VirtualCommandBuffer::BlitImage(
    VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage,
    VkImageLayout dstImageLayout, uint32_t regionCount,
    const VkImageBlit *pRegions, VkFilter filter) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdBlitImage);
  auto &commandInfo = std::get<Args::VkCmdBlitImage>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdCopyBuffer);
  auto &commandInfo = std::get<Args::VkCmdCopyBuffer>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdCopyImage);
  auto &commandInfo = std::get<Args::VkCmdCopyImage>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdCopyBufferToImage);
  auto &commandInfo = std::get<Args::VkCmdCopyBufferToImage>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdCopyImageToBuffer);
  auto &commandInfo = std::get<Args::VkCmdCopyImageToBuffer>(command.data);

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
  auto &command = GetNewCommand(CommandType::vkCmdFillBuffer);
  auto &commandInfo = std::get<Args::VkCmdFillBuffer>(command.data);

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
  auto &command =
      GetNewCommand(CommandType::vkCmdBuildAccelerationStructuresKHR);
  auto &commandInfo =
      std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);

  commandInfo.infoCount = infoCount;
  commandInfo.infos.resize(infoCount);
  commandInfo.buildRangeInfos.resize(infoCount);
  commandInfo.bufferReads.resize(readCount);
  commandInfo.bufferWrites.resize(writeCount);

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
      commandInfo.geometries.push_back(
          commandInfo.infos[i].pGeometries[j]); // NOLINT
    }

    commandInfo.buildRangeInfos[i] = *ppBuildRangeInfos[i]; // NOLINT
  }

  return AddCommand(command);
}

auto VirtualCommandBuffer::CopyAccelerationStructureKHR(
    const VkCopyAccelerationStructureInfoKHR *pInfo) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdCopyAccelerationStructureKHR);
  auto &commandInfo =
      std::get<Args::VkCmdCopyAccelerationStructureKHR>(command.data);

  commandInfo.structureInfo = *pInfo;

  return AddCommand(command);
}

auto VirtualCommandBuffer::PipelineBarrier2(
    const VkDependencyInfo *pDependencyInfo) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdPipelineBarrier2);
  auto &commandInfo = std::get<Args::VkCmdPipelineBarrier2>(command.data);

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
  auto &command = GetNewCommand(CommandType::mipmapTexture);
  auto &commandInfo = std::get<Args::MipmapTexture>(command.data);

  commandInfo.texture = texture;

  return AddCommand(command);
}

auto VirtualCommandBuffer::ResetQueryPool(VkQueryPool queryPool,
                                          uint32_t firstQuery,
                                          uint32_t queryCount) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdResetQueryPool);
  auto &commandInfo = std::get<Args::VkCmdResetQueryPool>(command.data);

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
  auto &command =
      GetNewCommand(CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR);
  auto &commandInfo =
      std::get<Args::VkCmdWriteAccelerationStructuresPropertiesKHR>(
          command.data);

  commandInfo.accelerationStructures.resize(accelerationStructureCount);
  memcpy(commandInfo.accelerationStructures.data(), // NOLINT
         pAccelerationStructures,                   // NOLINT
         sizeof(VkAccelerationStructureKHR) * accelerationStructureCount);
  commandInfo.queryType = queryType;
  commandInfo.queryPool = queryPool;
  commandInfo.firstQuery = firstQuery;

  return AddCommand(command);
}

auto VirtualCommandBuffer::PushConstants(VkPipelineLayout layout,
                                         VkShaderStageFlags stageFlags,
                                         uint32_t offset, uint32_t size,
                                         const void *pValues) -> void {
  assert(offset == 0 && "Offset pushconstants are currently not supported.");
  currentState.pushConstants.resize(size);
  memcpy(currentState.pushConstants.data(), pValues, size);
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

  currentState.bindingDescriptions.resize(vertexBindingDescriptionCount);
  currentState.attributeDescriptions.resize(vertexAttributeDescriptionCount);

  memcpy(currentState.bindingDescriptions.data(), pVertexBindingDescriptions,
         sizeof(VkVertexInputBindingDescription2EXT) *
             vertexBindingDescriptionCount);

  memcpy(currentState.attributeDescriptions.data(),
         pVertexAttributeDescriptions,
         sizeof(VkVertexInputBindingDescription2EXT) *
             vertexAttributeDescriptionCount);
}

auto VirtualCommandBuffer::SetViewport(uint32_t firstViewport,
                                       uint32_t viewportCount,
                                       const VkViewport *pViewports) -> void {
  assert(viewportCount == 1);
  assert(firstViewport == 0);
  currentState.viewport = *pViewports;

  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetScissor(const Args::VkCmdSetScissor &arguments)
    -> void {
  currentState.scissor = arguments.scissors.front();
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetDepthTestEnable(
    const Args::VkCmdSetDepthTestEnable &arguments) -> void {
  currentState.depthTestEnable = arguments.depthTestEnable;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetDepthWriteEnable(
    const Args::VkCmdSetDepthWriteEnable &arguments) -> void {
  currentState.depthWriteEnable = arguments.depthWriteEnable;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetDepthCompareOp(
    const Args::VkCmdSetDepthCompareOp &arguments) -> void {
  currentState.depthCompareOp = arguments.depthCompareOp;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetColorBlendEquationEXT(
    uint32_t firstAttachment, uint32_t attachmentCount,
    const VkColorBlendEquationEXT *pColorBlendEquations) -> void {
  currentState.colorBlendEquations.resize(firstAttachment + attachmentCount);
  memcpy(currentState.colorBlendEquations.data() + firstAttachment, // NOLINT
         pColorBlendEquations,
         sizeof(VkColorBlendEquationEXT) * attachmentCount);
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetCullMode(const Args::VkCmdSetCullMode &arguments)
    -> void {
  currentState.cullMode = arguments.cullMode;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::SetFrontFace(
    const Args::VkCmdSetFrontFace &arguments) -> void {
  currentState.frontFace = arguments.frontFace;
  currentState.MarkUpdated();
}

auto VirtualCommandBuffer::ClearAttachments(
    uint32_t attachmentCount, const VkClearAttachment *pAttachments,
    uint32_t rectCount, const VkClearRect *pRects) -> Error {
  auto &command = GetNewCommand(CommandType::vkCmdClearAttachments);
  auto &commandInfo = std::get<Args::VkCmdClearAttachments>(command.data);

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

  for (const auto &command : commands) {
    caches.at((uint8_t)command.type).emplace_back(command);
  }

  commands.clear();
  queueFamily = UINT32_MAX;
}

auto LoadOpConfig::FromGraphState(const GraphState &graphState,
                                  bool forceLoadOpLoad) -> LoadOpConfig {
  LoadOpConfig config{};

  for (const auto &attachment : graphState.colorAttachments) {
    config.loadOps.emplace_back(forceLoadOpLoad ? VK_ATTACHMENT_LOAD_OP_LOAD
                                                : attachment.loadOp);
  }

  if (graphState.hasDepthStencilAttachment) {
    config.depthStencilLoadOp = forceLoadOpLoad
                                    ? VK_ATTACHMENT_LOAD_OP_LOAD
                                    : graphState.depthStencilAttachment.loadOp;
  }

  return config;
}

auto DrawState::Apply(const GraphicsContext &context, VkCommandBuffer cmdBuffer,
                      const LoadOpConfig *loadConfig) const -> Error {
  ZoneScoped;
  ERR_ASSERT(stateID != UINT32_MAX);

  const auto &state = CommandStateManager::States.at(stateID);

  RecordingState::CurrentState.bindPoint = state.bindPoint;
  RecordingState::CurrentState.shader = CHECK_NULL(state.shader);

  if (CommandStateManager::CurrentStateID != stateID) {
    RecordingState::CurrentState.MarkUpdated();
  }

  if (RecordingState::CurrentState.bindPoint ==
      VK_PIPELINE_BIND_POINT_GRAPHICS) {
    ZoneScopedN("Set vertex input");

    if (CommandStateManager::CurrentStateID != stateID) {
      ZoneScopedN("Update state");

      RecordingState::CurrentState.colorAttachments = state.colorAttachments;
      RecordingState::CurrentState.depthStencilAttachment =
          state.depthStencilAttachment;
      RecordingState::CurrentState.hasDepthStencilAttachment =
          state.hasDepthStencilAttachment;
      RecordingState::CurrentState.primitiveTopology = state.primitiveTopology;
      RecordingState::CurrentState.colorBlendEquations =
          state.colorBlendEquations;
      RecordingState::CurrentState.cullMode = state.cullMode;
      RecordingState::CurrentState.frontFace = state.frontFace;
      RecordingState::CurrentState.depthTestEnable = state.depthTestEnable;
      RecordingState::CurrentState.depthWriteEnable = state.depthWriteEnable;
      RecordingState::CurrentState.depthCompareOp = state.depthCompareOp;
      RecordingState::CurrentState.stencilTestEnable = state.stencilTestEnable;
      RecordingState::CurrentState.polygonMode = state.polygonMode;
      RecordingState::CurrentState.viewport = state.viewport;
      RecordingState::CurrentState.scissor = state.scissor;

      vkCmdSetVertexInputEXT(cmdBuffer, state.bindingDescriptions.size(),
                             state.bindingDescriptions.data(),
                             state.attributeDescriptions.size(),
                             state.attributeDescriptions.data());
    }

    if (!vertexBuffers.empty()) {
      vkCmdBindVertexBuffers(cmdBuffer, 0, vertexBuffers.size(),
                             vertexBuffers.data(), vertexBufferOffsets.data());
    }

    if (indexBuffer != VK_NULL_HANDLE) {
      vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexBufferOffset,
                           indexType);
    }
  }

  if (CommandStateManager::CurrentStateID != stateID) {
    CHECK_ERR(PrepareRendering(context, cmdBuffer, loadConfig));
  }

  CommandStateManager::CurrentStateID = stateID;

  assert(GetPipelineCache().currentLayout.layout);
  assert(cmdBuffer);

  if (state.shader->pushBuffer) {
    ZoneScopedN("Flush push buffer data");

    auto &pushBuffer = state.shader->pushBuffer;
    CHECK_ERR(pushBuffer->SetData(pushConstants));
    pushBuffer->FlushData(GetPipelineCache().currentLayout.layout, cmdBuffer);
  }

  if (!descriptorSets.empty()) {
    ZoneScopedN("Bind descriptor sets");

    vkCmdBindDescriptorSets(cmdBuffer, state.bindPoint,
                            GetPipelineCache().currentLayout.layout, 0,
                            descriptorSets.size(), descriptorSets.data(),
                            dynamicOffsets.size(), dynamicOffsets.data());
  }

  return {};
}

} // namespace Graphics