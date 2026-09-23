#include "dynamicRendering.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/renderState.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "pipelineState.hpp"
#include <public/tracy/Tracy.hpp>
#include <vulkan/vulkan_core.h>

namespace Graphics {

inline auto GetRenderExtent(const GraphicsContext &context,
                            const RenderState::State &state) -> VkExtent2D {

  if (!state.colorAttachments.empty()) {
    return VkExtent2D{
        .width = state.colorAttachments.front().texture->GetWidth(),
        .height = state.colorAttachments.front().texture->GetHeight(),
    };
  }

  if (state.hasDepthStencilAttachment) {
    return VkExtent2D{
        .width = state.depthStencilAttachment.texture->GetWidth(),
        .height = state.depthStencilAttachment.texture->GetHeight(),
    };
  }

  assert(false && "Trying to get render extent with no attachments.");
  return VkExtent2D{0, 0};
}

auto compareScissors(const RenderState::State &first,
                     const RenderState::State &second) -> bool {
  return first.scissor.offset.x == second.scissor.offset.x &&
         first.scissor.offset.y == second.scissor.offset.y &&
         first.scissor.extent.width == second.scissor.extent.width &&
         first.scissor.extent.height == second.scissor.extent.height;
}

auto compareViewports(const RenderState::State &first,
                      const RenderState::State &second) -> bool {
  return first.viewport.x == second.viewport.x &&
         first.viewport.y == second.viewport.y &&
         first.viewport.width == second.viewport.width &&
         first.viewport.height == second.viewport.height;
}

auto compareDepthConfigs(const RenderState::State &first,
                         const RenderState::State &second) -> bool {
  return first.depthTestEnable == second.depthTestEnable &&
         first.depthWriteEnable == second.depthWriteEnable &&
         first.depthCompareOp == second.depthCompareOp;
}

auto compareBlendmodes(const RenderState::State &first,
                       const RenderState::State &second) -> bool {
  if (first.colorAttachments.size() != second.colorAttachments.size()) {
    return false;
  }

  for (size_t i = 0; i < first.colorAttachments.size(); i++) {
    if (!RenderState::CompareVkPipelineColorBlendAttachmentState(
            first.colorAttachments.at(i).blendMode,
            second.colorAttachments.at(i).blendMode)) {
      return false;
    }
  }

  if (first.hasDepthStencilAttachment != second.hasDepthStencilAttachment) {
    return false;
  }

  if (first.hasDepthStencilAttachment && second.hasDepthStencilAttachment) {
    if (first.depthStencilAttachment.blendMode.blendEnable !=
        second.depthStencilAttachment.blendMode.blendEnable) {
      return false;
    }
  }

  return true;
}

// NOLINTNEXTLINE
inline auto BeginRendering(const GraphicsContext &context,
                           VkCommandBuffer cmdBuffer,
                           const LoadOpConfig &loadConfig) -> Error {
  ZoneScoped;

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  renderingInfo.renderArea.offset = {.x = 0, .y = 0};
  renderingInfo.renderArea.extent =
      GetRenderExtent(context, RecordingState::CurrentState);
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  if (RecordingState::CurrentState.colorAttachments.size() >
      context.deviceProperties.limits.maxColorAttachments) {
    return Error::Create(
        "Number of bound render targets exceeds device limits.");
  }

  if (renderingInfo.renderArea.extent.width == 0 ||
      renderingInfo.renderArea.extent.height == 0) {
    return Error::Create("Render area has zero width or height.");
  }

  auto colorAttachments =
      std::array<VkRenderingAttachmentInfo, MAX_COLOR_ATTACHMENTS>{};
  auto depthAttachment = VkRenderingAttachmentInfo{};
  auto stencilAttachment = VkRenderingAttachmentInfo{};

  for (int i = 0; i < RecordingState::CurrentState.colorAttachments.size();
       i++) {
    const auto &rendertarget =
        RecordingState::CurrentState.colorAttachments.at(i);
    CHECK_ERR(rendertarget.texture->UseAsAttachment(context,
                                                    loadConfig.loadOps.at(i)));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachmentInfo.loadOp = loadConfig.loadOps.at(i);
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    colorAttachments.at(i) = attachmentInfo;
  }

  bool hasDepth = false;
  bool hasStencil = false;

  if (RecordingState::CurrentState.hasDepthStencilAttachment) {
    auto &rendertarget = RecordingState::CurrentState.depthStencilAttachment;
    CHECK_ERR(
        rendertarget.texture->UseAsAttachment(context, rendertarget.loadOp));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachmentInfo.loadOp = loadConfig.depthStencilLoadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    if (rendertarget.texture->IsDepthTexture()) {
      depthAttachment = attachmentInfo;
      hasDepth = true;
    }

    if (rendertarget.texture->IsStencilTexture()) {
      stencilAttachment = attachmentInfo;
      hasStencil = true;
    }
  }

#ifndef NDEBUG
  VkExtent2D expectedExtent =
      GetRenderExtent(context, RecordingState::CurrentState);

  for (const auto &rendertarget :
       RecordingState::CurrentState.colorAttachments) {
    if (rendertarget.texture->GetWidth() != expectedExtent.width ||
        rendertarget.texture->GetHeight() != expectedExtent.height) {
      return Error::Create(
          "Color attachment extent does not match render area extent.");
    }
  }

  if (hasDepth || hasStencil) {
    if (RecordingState::CurrentState.depthStencilAttachment.texture
                ->GetWidth() != expectedExtent.width ||
        RecordingState::CurrentState.depthStencilAttachment.texture
                ->GetHeight() != expectedExtent.height) {
      return Error::Create(
          "Depth/stencil attachment extent does not match render area extent.");
    }
  }
#endif

  renderingInfo.colorAttachmentCount =
      RecordingState::CurrentState.colorAttachments.size();
  renderingInfo.pColorAttachments = colorAttachments.data();

  renderingInfo.pStencilAttachment = hasStencil ? &stencilAttachment : nullptr;
  renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : nullptr;

  // Add a tracy marker to indicate the start of a rendering pass
  TracyMessageL("Begin Rendering");
  vkCmdBeginRendering(cmdBuffer, &renderingInfo);
  GetIsCurrentlyRendering() = true;

  // Make sure subsequent renders load from the existing content if we ever need to re-bind mid-pass
  for (auto &rendertarget : RecordingState::CurrentState.colorAttachments) {
    rendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  if (RecordingState::CurrentState.hasDepthStencilAttachment) {
    RecordingState::CurrentState.depthStencilAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_LOAD;
  }

  return Error::Success();
}

auto EndRendering(const GraphicsContext &context,
                  VkCommandBuffer vkCommandBuffer) -> void {
  if (GetIsCurrentlyRendering()) {

    // #if Enable_Snapshots
    //     Snapshot::CaptureEvent(Snapshot::EndRenderingEvent());
    // #endif

    TracyMessageL("End Rendering");
    vkCmdEndRendering(vkCommandBuffer);
    // GetCommandBuffer()->EndRendering({});
    GetIsCurrentlyRendering() = false;
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto PrepareRendering(const GraphicsContext &context,
                      VkCommandBuffer vkCommandBuffer,
                      const LoadOpConfig *loadConfig) -> Error {
  ZoneScoped;

  bool sameViewport = false;
  bool sameScissor = false;
  bool sameDepth = false;
  bool sameBlendMode = false;
  bool sameCullmode = false;
  bool sameFFWinding = false;

  // Flush updates the last state so we need to compare before updating it
  if (RecordingState::LastState != nullptr) {
    sameViewport = compareViewports(RecordingState::CurrentState,
                                    *RecordingState::LastState);
    sameScissor = compareScissors(RecordingState::CurrentState,
                                  *RecordingState::LastState);
    sameDepth = compareDepthConfigs(RecordingState::CurrentState,
                                    *RecordingState::LastState);
    sameBlendMode = compareBlendmodes(RecordingState::CurrentState,
                                      *RecordingState::LastState);
    sameCullmode = RecordingState::CurrentState.cullMode ==
                   RecordingState::LastState->cullMode;
    sameFFWinding = RecordingState::CurrentState.frontFace ==
                    RecordingState::LastState->frontFace;
  }

  bool isGraphics =
      RecordingState::CurrentState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

  auto updatedState = CHECK_RES(Flush(context, vkCommandBuffer));
  if (updatedState || !isGraphics) {
    EndRendering(context, vkCommandBuffer);
  }

  const auto stages =
      RecordingState::CurrentState.shader->combinedPipelineStages;

  bool wasRendering = GetIsCurrentlyRendering();

  if (isGraphics) {
    ZoneScopedN("Dynamic state setup");

    if (!wasRendering) {
      CHECK_NULL(loadConfig);
      CHECK_ERR(BeginRendering(context, vkCommandBuffer, *loadConfig));
    }

    if (!sameViewport) {
      auto viewport = GetClippedViewport();
      vkCmdSetViewport(vkCommandBuffer, 0, 1, &viewport);
    }

    if (!sameScissor) {
      auto scissor = GetScissor();
      vkCmdSetScissor(vkCommandBuffer, 0, 1, &scissor);
    }

    if (!sameDepth) {
      vkCmdSetDepthTestEnable(vkCommandBuffer,
                              RecordingState::CurrentState.depthTestEnable);
      vkCmdSetDepthWriteEnable(vkCommandBuffer,
                               RecordingState::CurrentState.depthWriteEnable);
      vkCmdSetDepthCompareOp(vkCommandBuffer,
                             RecordingState::CurrentState.depthCompareOp);
    }

    if (!sameBlendMode &&
        RecordingState::CurrentState.colorAttachments.size() > 0) {
      Math::StackVector<VkBool32, MAX_COLOR_ATTACHMENTS> blendEnables = {};
      Math::StackVector<VkColorComponentFlags, MAX_COLOR_ATTACHMENTS>
          colorWriteMasks = {};
      for (const auto &attachment :
           RecordingState::CurrentState.colorAttachments) {
        blendEnables.emplace_back(attachment.blendMode.blendEnable);
        colorWriteMasks.emplace_back(attachment.blendMode.colorWriteMask);
      }

      vkCmdSetColorBlendEquationEXT(
          vkCommandBuffer, 0,
          static_cast<uint32_t>(
              RecordingState::CurrentState.colorBlendEquations.size()),
          RecordingState::CurrentState.colorBlendEquations.data());
      vkCmdSetColorBlendEnableEXT(vkCommandBuffer, 0,
                                  static_cast<uint32_t>(blendEnables.size()),
                                  blendEnables.data());
      vkCmdSetColorWriteMaskEXT(vkCommandBuffer, 0,
                                static_cast<uint32_t>(colorWriteMasks.size()),
                                colorWriteMasks.data());
    }

    if (!sameCullmode) {
      vkCmdSetCullMode(vkCommandBuffer, RecordingState::CurrentState.cullMode);
    }

    if (!sameFFWinding) {
      vkCmdSetFrontFace(vkCommandBuffer,
                        RecordingState::CurrentState.frontFace);
    }
  }

  Graphics::GetIsStateDirty() = false;
  return Error::Success();
}

} // namespace Graphics