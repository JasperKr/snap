#include "commands.hpp"
#include "Graphics/FrameGraph/descriptorState.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/vkAccessHelpers.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "dynamicRendering.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <public/tracy/Tracy.hpp>
#include <tuple>
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

auto DrawState::GetStateFor(const VulkanResource &resource,
                            CommandType type) const
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {

  SyncFlags flags{};

  static const auto getFlags =
      [](const std::vector<BoundResource> &boundResources,
         const VulkanResource &resource) -> SyncFlags {
    VkAccessFlags2 access = 0;
    VkPipelineStageFlags2 pipelines = 0;

    for (const auto &bound : boundResources) {
      if (bound.Overlaps(resource)) {
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
    for (int i = 0; i < colorAttachments.size(); i++) {
      const auto &attachment = colorAttachments.at(i);

      if (attachment.Overlaps(resource)) {
        const bool blendEnabled =
            graphState.colorAttachments.at(i).blendMode.blendEnable != 0U;

        if (blendEnabled) {
          flags.access |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        }

        flags.access |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        flags.pipelines |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      }
    }

    if (depthStencilAttachment.has_value() &&
        depthStencilAttachment->Overlaps(resource)) {
      if (graphState.depthTestEnable != 0U) {
        flags.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        flags.pipelines |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
      }

      if (graphState.depthWriteEnable != 0U) {
        flags.access |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        flags.pipelines |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      }
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
      if (bound.Overlaps(resource) && IsReadAccess(bound.access)) {
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
      if (bound.Overlaps(resource) && IsWriteAccess(bound.access)) {
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

auto DrawState::Initialize(const GraphicsContext &context, CommandType type)
    -> Error {
  const auto &shader = RenderState::GetShader();

  const auto &ctx = GetThreadContext();
  auto &graphState = ctx.commandBuffer->GetGraphState();
  graphState.shader = shader;
  graphState.bindPoint = RenderState::GetBindPoint();

  if (shader->pushBuffer) {
    const auto &data = shader->pushBuffer->GetData();
    pushConstants.resize(data.size());
    memcpy(pushConstants.data(), data.data(), data.size());
  }

  // PrintAlways("Shader: {}", shader->moduleName);
  // PrintAlways("Draw state with bind point: {}", (int)graphState.bindPoint);

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
  }
  graphState.MarkUpdated();

  if (!isCompute) {
    const auto &rendertargets = RenderState::GetRenderTargets();

    VkAccessFlags2 depthAccess = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 depthPipelines = VK_PIPELINE_STAGE_2_NONE;

    // clang-format off
    depthAccess |= (graphState.depthTestEnable != 0U) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT : 0U;
    depthAccess |= (graphState.depthWriteEnable != 0U) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0U;

    depthPipelines |= (graphState.depthTestEnable != 0U) ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT : 0U;
    depthPipelines |= (graphState.depthWriteEnable != 0U) ? VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT : 0U;
    // clang-format on

    for (const auto &target : rendertargets) {
      if (Image::IsDepthOrStencilTexture(target.texture->GetFormat())) {
        depthStencilAttachment =
            BoundResource{.resource = ImageSubresource{target.texture},
                          .access = depthAccess,
                          .pipelines = depthPipelines};
      } else {
        colorAttachments.emplace_back(BoundResource{
            .resource = ImageSubresource{target.texture},
            // clang-format off
            .access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | ((target.blendMode.blendEnable != 0U) ? VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT : 0U),
            .pipelines = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            // clang-format on
        });
      }
    }
  }

  const auto &shaderState = shader->GetState();
  const auto &pipelines = shader->combinedPipelineStages;

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

  vertexBuffers.insert(vertexBuffers.begin(), graphState.vertexBuffers.begin(),
                       graphState.vertexBuffers.end());
  indexBuffer = graphState.indexBuffer;
  vertexBufferOffsets.insert(vertexBufferOffsets.begin(),
                             graphState.vertexBufferOffsets.begin(),
                             graphState.vertexBufferOffsets.end());
  indexBufferOffset = graphState.indexBufferOffset;
  indexType = graphState.indexType;

  stateID = ctx.commandBuffer->GetStateID();

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

auto VirtualCommandBuffer::AddCommand(const Command &command) -> Error {
  commands.emplace_back(command);
  time++;

  auto *state = commands.back().GetDrawState();
  if (state != nullptr) {
    return state->Initialize(*GetCurrentGraphicsContext(), command.GetType());
  }

  return {};
}

auto VirtualCommandBuffer::Draw(const Args::VkCmdDraw &arguments) -> Error {
  auto command = Command(arguments);
  return AddCommand(command);
}

auto VirtualCommandBuffer::DrawIndexed(const Args::VkCmdDrawIndexed &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DrawIndirect(
    const Args::VkCmdDrawIndirect &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DrawIndexedIndirect(
    const Args::VkCmdDrawIndexedIndirect &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::Dispatch(const Args::VkCmdDispatch &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::DispatchIndirect(
    const Args::VkCmdDispatchIndirect &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BlitImage(const Args::VkCmdBlitImage &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::PushConstants(
    const Args::VkCmdPushConstants &arguments) -> void {
  currentState.pushConstants = arguments.values;
}

auto VirtualCommandBuffer::CopyBuffer(const Args::VkCmdCopyBuffer &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyImage(const Args::VkCmdCopyImage &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyBufferToImage(
    const Args::VkCmdCopyBufferToImage &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyImageToBuffer(
    const Args::VkCmdCopyImageToBuffer &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::MipmapTexture(const Args::MipmapTexture &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::FillBuffer(const Args::VkCmdFillBuffer &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BuildAccelerationStructuresKHR(
    const Args::VkCmdBuildAccelerationStructuresKHR &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::CopyAccelerationStructureKHR(
    const Args::VkCmdCopyAccelerationStructureKHR &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::ResetQueryPool(
    const Args::VkCmdResetQueryPool &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::WriteAccelerationStructuresPropertiesKHR(
    const Args::VkCmdWriteAccelerationStructuresPropertiesKHR &arguments)
    -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BindIndexBuffer(
    const Args::VkCmdBindIndexBuffer &arguments) -> void {
  currentState.indexBuffer = arguments.buffer;
  currentState.indexType = arguments.indexType;
  currentState.indexBufferOffset = arguments.offset;
}

auto VirtualCommandBuffer::BindVertexBuffers(
    const Args::VkCmdBindVertexBuffers &arguments) -> void {

  const auto first = arguments.firstBinding;
  const auto count = arguments.buffers.size();
  currentState.vertexBuffers.resize(first + count);
  currentState.vertexBufferOffsets.resize(first + count);

  for (size_t i = 0; i < count; ++i) {
    currentState.vertexBuffers[first + i] = arguments.buffers[i];
    currentState.vertexBufferOffsets[first + i] = arguments.offsets[i];
  }
}

auto VirtualCommandBuffer::SetVertexInputEXT(
    const Args::VkCmdSetVertexInputEXT &arguments) -> void {
  currentState.bindingDescriptions = arguments.bindingDescriptions;
  currentState.attributeDescriptions = arguments.attributeDescriptions;
}

auto VirtualCommandBuffer::BindPipeline(
    const Args::VkCmdBindPipeline &arguments) -> void {}

auto VirtualCommandBuffer::BindDescriptorSets(
    const Args::VkCmdBindDescriptorSets &arguments) -> void {}

auto VirtualCommandBuffer::SetViewport(const Args::VkCmdSetViewport &arguments)
    -> void {
  currentState.viewport = arguments.viewports.front();
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
    const Args::VkCmdSetColorBlendEquationEXT &arguments) -> void {
  currentState.colorBlendEquations =
      Math::StackVector<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS>(
          arguments.equations);
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
    const Args::VkCmdClearAttachments &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto VirtualCommandBuffer::BeginDebugUtilsLabelEXT(
    const Args::VkCmdBeginDebugUtilsLabelEXT &arguments) -> void {
  // currentState.currentDebugMarker = arguments.labelInfo.pLabelName;
}

auto VirtualCommandBuffer::EndDebugUtilsLabelEXT(
    const Args::VkCmdEndDebugUtilsLabelEXT &arguments) -> void {}

auto VirtualCommandBuffer::InsertDebugUtilsLabelEXT(
    const Args::VkCmdInsertDebugUtilsLabelEXT &arguments) -> void {}

auto VirtualCommandBuffer::PipelineBarrier2(
    const Args::VkCmdPipelineBarrier2 &arguments) -> Error {
  return AddCommand(Command(arguments));
}

auto CreateCommandBuffer() -> VirtualCommandBuffer { return {}; }

auto VirtualCommandBuffer::Reset() -> void {
  time = 0;
  commands.clear();
  queueFamily = UINT32_MAX;
}

auto ResetCommandBuffer(VirtualCommandBuffer &buffer) -> void {
  buffer.Reset();
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
  ERR_ASSERT(stateID != UINT32_MAX);

  const auto &state = CommandStateManager::States.at(stateID);

  RecordingState::CurrentState.bindPoint = state.bindPoint;

  RecordingState::CurrentState.shader = CHECK_NULL(state.shader);

  if (RecordingState::CurrentState.bindPoint ==
      VK_PIPELINE_BIND_POINT_GRAPHICS) {
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

    if (!vertexBuffers.empty()) {
      vkCmdBindVertexBuffers(cmdBuffer, 0, vertexBuffers.size(),
                             vertexBuffers.data(), vertexBufferOffsets.data());
    }

    if (indexBuffer != VK_NULL_HANDLE) {
      vkCmdBindIndexBuffer(cmdBuffer, indexBuffer, indexBufferOffset,
                           indexType);
    }
  }

  RecordingState::CurrentState.MarkUpdated();
  CommandStateManager::CurrentStateID = stateID;

  CHECK_ERR(PrepareRendering(context, cmdBuffer, loadConfig));

  if (state.shader->pushBuffer) {
    ZoneScopedN("Flush push buffer data");
    CHECK_NULL(GetPipelineCache().currentLayout.layout);

    auto &pushBuffer = state.shader->pushBuffer;
    CHECK_ERR(pushBuffer->SetData(pushConstants));
    pushBuffer->FlushData(GetPipelineCache().currentLayout.layout, cmdBuffer);
  }

  if (!descriptorSets.empty()) {
    vkCmdBindDescriptorSets(cmdBuffer, state.bindPoint,
                            GetPipelineCache().currentLayout.layout, 0,
                            descriptorSets.size(), descriptorSets.data(),
                            dynamicOffsets.size(), dynamicOffsets.data());
  }

  return {};
}

} // namespace Graphics