#include "renderState.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/FrameGraph/commands.hpp"

#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include <algorithm>
#include <vector>

#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstdint>

#include "../external/tracy/public/tracy/Tracy.hpp"

namespace Graphics::RenderState {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables, readability-function-cognitive-complexity, cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

thread_local std::vector<State> StateStack{};
thread_local State LastStateStorage;
thread_local State *LastState = nullptr;
thread_local State *TopOfStack = nullptr;

thread_local Stats CurrentStats;

auto CompareVkPipelineColorBlendAttachmentState(
    const VkPipelineColorBlendAttachmentState &first,
    const VkPipelineColorBlendAttachmentState &second) -> bool {
  return first.blendEnable == second.blendEnable &&
         first.srcColorBlendFactor == second.srcColorBlendFactor &&
         first.dstColorBlendFactor == second.dstColorBlendFactor &&
         first.colorBlendOp == second.colorBlendOp &&
         first.srcAlphaBlendFactor == second.srcAlphaBlendFactor &&
         first.dstAlphaBlendFactor == second.dstAlphaBlendFactor &&
         first.alphaBlendOp == second.alphaBlendOp &&
         first.colorWriteMask == second.colorWriteMask;
}

inline auto SetupDefaultState(const GraphicsContext &context) -> Result<State> {
  auto defaultState = State();

  defaultState.viewport = {};
  defaultState.scissor = {};

  defaultState.shader = DefaultShaderModule;

  auto const &texture =
      context.swapchainInfo.textures[context.swapchainImageIndex];

  RenderTarget swapchainRendertarget{};

  swapchainRendertarget.texture = texture;
  swapchainRendertarget.location = 0;
  swapchainRendertarget.blendMode = DefaultBlendMode;
  swapchainRendertarget.clearValue = {0.0F, 0.0F, 0.0F, 1.0F};
  swapchainRendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  PrintDebug("Setup default state with swapchain handle: {}",
             (void *)texture->view);

  defaultState.colorAttachments.emplace_back(swapchainRendertarget);
  defaultState.hasDepthStencilAttachment = false;

  return defaultState;
}

auto Load(const GraphicsContext &context) -> Error {
  assert(StateStack.size() == 0 &&
         "RenderTarget state stack is not empty on Load.");

  auto state = CHECK_RES(SetupDefaultState(context));

  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();

  return Error::Success();
}

auto Push(const GraphicsContext &context) -> Error {
  if (StateStack.size() == 0) {
    auto state = CHECK_RES(SetupDefaultState(context));
    StateStack.emplace_back(state);
  } else {
    StateStack.emplace_back(*TopOfStack);
  }
  TopOfStack = &StateStack.back();

  return Error::Success();
}

auto Pop(const GraphicsContext &context) -> Error {
  if (StateStack.size() <= 1 || StateStack.empty()) {
    return Error::Create("More pops than pushes.");
  }

  StateStack.pop_back();
  TopOfStack = &StateStack.back();

  return Error::Success();
}

auto Reset(const GraphicsContext &context) -> Error {
  StateStack.clear();
  CurrentStats.Reset();

  auto state = CHECK_RES(SetupDefaultState(context));

  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();
  LastState = nullptr;

  return Error::Success();
}

void Shutdown(const GraphicsContext &context) {
  StateStack.clear();
  CurrentStats.Reset();
  LastState = nullptr;
  TopOfStack = nullptr;
  LastStateStorage = State();
  QuadMesh.reset();
}

auto IsSwapchainTexture(const GraphicsContext &context,
                        const Graphics::Texture &texture) -> bool {
  for (const auto &swapchainTexture : context.swapchainInfo.textures) {
    if (swapchainTexture.get() == &texture) {
      return true;
    }
  }

  return false;
}

auto Destroy(const GraphicsContext &context) -> void {}

auto FinalizeFrame(const GraphicsContext &context) -> Error {
  ZoneScoped;

  [[unlikely]]
  if (StateStack.size() != 1) {
    return Error::Create("More pushes than pops.");
  }

  for (auto &rendertarget : TopOfStack->colorAttachments) {
    [[unlikely]]
    if (!IsSwapchainTexture(context, *rendertarget.texture)) {
      return Error::Create(
          "Non-swapchain render targets remain bound at end of frame.");
    }
  }

  auto &frameUbo = GetGlobalUniformBuffer(context.frameIndex);
  CHECK_ERR(frameUbo.Finalize(context));

  return Error::Success();
}

auto BeginFrame(const GraphicsContext &context) -> Error {
  StateStack.clear();
  CurrentStats.Reset();

  auto state = CHECK_RES(SetupDefaultState(context));
  StateStack.emplace_back(state);
  TopOfStack = &StateStack.back();
  LastState = nullptr;
  LastStateStorage = state;

  return Error::Success();
}

// Setters //

auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp)
    -> void {
  if (TopOfStack->depthTestEnable == static_cast<VkBool32>(enable) &&
      TopOfStack->depthWriteEnable == static_cast<VkBool32>(writeEnable) &&
      TopOfStack->depthCompareOp == compareOp) {
    return;
  }

  TopOfStack->depthTestEnable = static_cast<VkBool32>(enable);
  TopOfStack->depthWriteEnable = static_cast<VkBool32>(writeEnable);
  TopOfStack->depthCompareOp = compareOp;

  TopOfStack->MarkUpdated();
}

auto SetCullMode(VkCullModeFlags cullMode) -> void {
  if (TopOfStack->cullMode == cullMode) {
    return;
  }

  TopOfStack->cullMode = cullMode;
  TopOfStack->MarkUpdated();
}

auto SetPolygonMode(VkPolygonMode polygonMode) -> void {
  if (TopOfStack->polygonMode == polygonMode) {
    return;
  }

  TopOfStack->polygonMode = polygonMode;
  TopOfStack->MarkUpdated();
}

auto SetViewport(const VkViewport *viewport) -> void {
  if (viewport == nullptr) {
    if (!TopOfStack->hasViewport) {
      return;
    }

    TopOfStack->MarkUpdated();

    TopOfStack->hasViewport = false;
    return;
  }

  if (TopOfStack->hasViewport && TopOfStack->viewport.x == viewport->x &&
      TopOfStack->viewport.y == viewport->y &&
      TopOfStack->viewport.width == viewport->width &&
      TopOfStack->viewport.height == viewport->height) {
    return;
  }

  TopOfStack->MarkUpdated();

  TopOfStack->hasViewport = true;
  TopOfStack->viewport = *viewport;
}

auto SetScissor(const VkRect2D *scissor) -> void {
  if (scissor == nullptr) {
    if (!TopOfStack->hasScissor) {
      return;
    }

    TopOfStack->MarkUpdated();

    TopOfStack->hasScissor = false;
    return;
  }

  if (TopOfStack->hasScissor &&
      TopOfStack->scissor.offset.x == scissor->offset.x &&
      TopOfStack->scissor.offset.y == scissor->offset.y &&
      TopOfStack->scissor.extent.width == scissor->extent.width &&
      TopOfStack->scissor.extent.height == scissor->extent.height) {
    return;
  }

  TopOfStack->MarkUpdated();

  TopOfStack->hasScissor = true;
  TopOfStack->scissor = *scissor;
}

auto ClipScissor(const VkRect2D &scissor) -> void {
  TopOfStack
      ->MarkUpdated(); // We just assume the scissor is changing, so mark the state as dirty
  auto &currentScissor = TopOfStack->scissor;

  int32_t rectMinX = std::max(currentScissor.offset.x, scissor.offset.x);
  int32_t rectMinY = std::max(currentScissor.offset.y, scissor.offset.y);
  int32_t rectMaxX =
      std::min(currentScissor.offset.x +
                   static_cast<int32_t>(currentScissor.extent.width),
               scissor.offset.x + static_cast<int32_t>(scissor.extent.width));
  int32_t rectMaxY =
      std::min(currentScissor.offset.y +
                   static_cast<int32_t>(currentScissor.extent.height),
               scissor.offset.y + static_cast<int32_t>(scissor.extent.height));

  if (rectMaxX < rectMinX || rectMaxY < rectMinY) {
    // No intersection, set to zero area
    currentScissor.offset.x = 0;
    currentScissor.offset.y = 0;
    currentScissor.extent.width = 0;
    currentScissor.extent.height = 0;
  } else {
    currentScissor.offset.x = rectMinX;
    currentScissor.offset.y = rectMinY;
    currentScissor.extent.width = static_cast<uint32_t>(rectMaxX - rectMinX);
    currentScissor.extent.height = static_cast<uint32_t>(rectMaxY - rectMinY);
  }
}

auto SetShader(const Ref<Shader> &shader) -> void {
  if (shader == nullptr) {
    if (TopOfStack->shader != nullptr) {
      TopOfStack->MarkUpdated();
    }

    TopOfStack->shader = DefaultShaderModule;
  } else {
    if (TopOfStack->shader != shader) {
      TopOfStack->MarkUpdated();
    }

    TopOfStack->shader = shader;
  }

  if ((TopOfStack->shader->combinedShaderStages &
       VK_SHADER_STAGE_COMPUTE_BIT) != 0) {
    RenderState::SetBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE);
  } else {
    RenderState::SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto SetRenderTargets(const GraphicsContext &context,
                      const std::vector<RenderTarget> &renderTargets) -> Error {
  TopOfStack->MarkUpdated();

  ERR_ASSERT(!renderTargets.empty());
  ERR_ASSERT(renderTargets.front().texture != nullptr);

  bool topHadDepthStencil = TopOfStack->hasDepthStencilAttachment;
  TopOfStack->hasDepthStencilAttachment = false;

  VkExtent2D expectedExtent = {renderTargets.front().texture->GetWidth(),
                               renderTargets.front().texture->GetHeight()};

  TopOfStack->colorAttachments.clear();
  TopOfStack->colorBlendEquations.clear();

  for (const auto &target : renderTargets) {
    ERR_ASSERT(target.texture != nullptr);
    ERR_ASSERT(target.texture->GetWidth() == expectedExtent.width);
    ERR_ASSERT(target.texture->GetHeight() == expectedExtent.height);

    // Depth/Stencil is separate
    if (target.texture->IsDepthOrStencilTexture()) {
      TopOfStack->hasDepthStencilAttachment = true;
      TopOfStack->depthStencilAttachment = target;
      continue;
    }

    TopOfStack->colorAttachments.emplace_back(target);
    TopOfStack->colorBlendEquations.emplace_back(VkColorBlendEquationEXT{
        .srcColorBlendFactor = target.blendMode.srcColorBlendFactor,
        .dstColorBlendFactor = target.blendMode.dstColorBlendFactor,
        .colorBlendOp = target.blendMode.colorBlendOp,
        .srcAlphaBlendFactor = target.blendMode.srcAlphaBlendFactor,
        .dstAlphaBlendFactor = target.blendMode.dstAlphaBlendFactor,
        .alphaBlendOp = target.blendMode.alphaBlendOp,
    });
  }

  SetViewport(nullptr);

  // Only clear if we have the same render targets as before
  // And have load ops that require clearing
  if (TopOfStack == LastState) {
    return Error::Success();
  }

  if (GetIsCurrentlyRendering()) {
    ClearInfo clearInfo{};
    for (const auto &target : TopOfStack->colorAttachments) {
      if (target.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR) {
        clearInfo.colors.emplace_back(target.clearValue.color.float32[0],
                                      target.clearValue.color.float32[1],
                                      target.clearValue.color.float32[2],
                                      target.clearValue.color.float32[3]);
      }
    }

    if (TopOfStack->hasDepthStencilAttachment &&
        TopOfStack->depthStencilAttachment.loadOp ==
            VK_ATTACHMENT_LOAD_OP_CLEAR) {
      clearInfo.clearDepth = true;
      clearInfo.depthClearValue =
          TopOfStack->depthStencilAttachment.clearValue.depthStencil.depth;
    }

    CHECK_ERR(Clear(context, clearInfo));
  }

  return Error::Success();
}

auto SetWindingOrder(VkFrontFace frontFace) -> void {
  if (TopOfStack->frontFace == frontFace) {
    return;
  }

  TopOfStack->frontFace = frontFace;
  TopOfStack->MarkUpdated();
}

auto SetTopology(VkPrimitiveTopology topology) -> void {
  if (TopOfStack->primitiveTopology == topology) {
    return;
  }

  TopOfStack->primitiveTopology = topology;
  TopOfStack->MarkUpdated();
}

// Getters //

auto GetDepthMode() -> std::tuple<bool, bool, VkCompareOp> {
  return {TopOfStack->depthTestEnable, TopOfStack->depthWriteEnable,
          TopOfStack->depthCompareOp};
}

auto GetCullMode() -> VkCullModeFlags { return TopOfStack->cullMode; }

auto GetPolygonMode() -> VkPolygonMode { return TopOfStack->polygonMode; }

auto GetUserShader() -> Ref<Shader> {
  if (TopOfStack->shader.get() == DefaultShaderModule.get()) {
    return Ref<Shader>(nullptr);
  }
  return TopOfStack->shader;
}

auto GetShader() -> Ref<Shader> { return TopOfStack->shader; }

auto GetScissor() -> VkRect2D {

  if (!TopOfStack->hasScissor) {
    VkRect2D scissor = {};
    auto viewport = GetMaximumAllowedViewport();
    scissor.offset = {.x = 0, .y = 0};
    scissor.extent = {
        .width = static_cast<uint32_t>(viewport.width),
        .height = static_cast<uint32_t>(viewport.height),
    };

    scissor.extent.width = (std::max)(scissor.extent.width, 1U);
    scissor.extent.height = (std::max)(scissor.extent.height, 1U);

    return scissor;
  }

  return {
      .offset =
          {
              .x = TopOfStack->scissor.offset.x,
              .y = TopOfStack->scissor.offset.y,
          },
      .extent =
          {
              .width = (std::max)(TopOfStack->scissor.extent.width, 1U),
              .height = (std::max)(TopOfStack->scissor.extent.height, 1U),
          },
  };
}

auto GetTargetSize() -> VkExtent2D {
  if (TopOfStack->colorAttachments.size() > 0) {
    return {
        TopOfStack->colorAttachments.at(0).texture->GetWidth(),
        TopOfStack->colorAttachments.at(0).texture->GetHeight(),
    };
  }

  if (TopOfStack->hasDepthStencilAttachment) {
    return {
        TopOfStack->depthStencilAttachment.texture->GetWidth(),
        TopOfStack->depthStencilAttachment.texture->GetHeight(),
    };
  }

  // No attachments, return zero size
  return {0, 0};
}

auto GetMaximumAllowedViewport() -> VkViewport {
  auto viewport = TopOfStack->viewport;

  auto size = GetTargetSize();

  viewport.width = static_cast<float>(size.width);
  viewport.height = static_cast<float>(size.height);

  viewport.width = (std::max)(viewport.width, 1.0F);
  viewport.height = (std::max)(viewport.height, 1.0F);

  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  return viewport;
}

auto GetViewport() -> VkViewport {

  if (!TopOfStack->hasViewport) {
    return GetMaximumAllowedViewport();
  }

  // return TopOfStack->viewport;
  return {
      .x = TopOfStack->viewport.x,
      .y = TopOfStack->viewport.y,
      .width = (std::max)(TopOfStack->viewport.width, 1.0F),
      .height = (std::max)(TopOfStack->viewport.height, 1.0F),
      .minDepth = TopOfStack->viewport.minDepth,
      .maxDepth = TopOfStack->viewport.maxDepth,
  };
}

auto GetClippedViewport() -> VkViewport {

  if (!TopOfStack->hasViewport) {
    return GetMaximumAllowedViewport();
  }

  auto viewport = TopOfStack->viewport;

  // Default to size of current attachments
  VkExtent2D size = GetTargetSize();

  viewport.width = std::min(viewport.width, static_cast<float>(size.width));
  viewport.height = std::min(viewport.height, static_cast<float>(size.height));

  return viewport;
}

auto GetRenderTargets() -> std::vector<RenderTarget> {
  std::vector<RenderTarget> targets = TopOfStack->colorAttachments.asVector();
  if (TopOfStack->hasDepthStencilAttachment) {
    targets.emplace_back(TopOfStack->depthStencilAttachment);
  }

  return targets;
}

auto GetWindingOrder() -> VkFrontFace { return TopOfStack->frontFace; }

auto GetTopology() -> VkPrimitiveTopology {
  return TopOfStack->primitiveTopology;
}

auto SetBindPoint(VkPipelineBindPoint bindPoint) -> void {
  TopOfStack->bindPoint = bindPoint;
}
auto GetBindPoint() -> VkPipelineBindPoint { return TopOfStack->bindPoint; }

auto Clear(const GraphicsContext &context, const ClearInfo &clearInfo)
    -> Error {
  ZoneScoped;

  auto *commandBuffer = Graphics::GetVirtualCommandBuffer();

  if (commandBuffer == nullptr) {
    return Error::Create("Command buffer is null in Clear.");
  }

  if (!clearInfo.clearDepth && !clearInfo.clearStencil &&
      clearInfo.colors.empty()) {
    return {};
  }

  Math::StackVector<VkClearAttachment, MAX_COLOR_ATTACHMENTS + 1>
      clearAttachments{};
  Math::StackVector<VkClearRect, MAX_COLOR_ATTACHMENTS> clearRects{};

  VkClearRect clearRect = {};
  auto viewport = GetClippedViewport();

  clearRect.rect.offset = {
      .x = static_cast<int32_t>(viewport.x),
      .y = static_cast<int32_t>(viewport.y),
  };
  clearRect.rect.extent = {
      .width = static_cast<uint32_t>(viewport.width),
      .height = static_cast<uint32_t>(viewport.height),
  };
  clearRect.baseArrayLayer = 0;
  clearRect.layerCount = 1;

  for (uint32_t i = 0; i < TopOfStack->colorAttachments.size(); i++) {
    VkClearAttachment clearAttachment = {};
    const auto &rendertarget = TopOfStack->colorAttachments.at(i);

    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = i;
    auto color = clearInfo.colors.size() > i ? clearInfo.colors[i]
                                             : Color{0.0F, 0.0F, 0.0F, 1.0F};
    clearAttachment.clearValue.color.float32[0] = color.r;
    clearAttachment.clearValue.color.float32[1] = color.g;
    clearAttachment.clearValue.color.float32[2] = color.b;
    clearAttachment.clearValue.color.float32[3] = color.a;

    clearAttachments.emplace_back(clearAttachment);
    clearRects.emplace_back(clearRect);
  }

  if (clearInfo.clearDepth) {
    VkClearAttachment clearAttachment = {};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clearAttachment.clearValue.depthStencil.depth = clearInfo.depthClearValue;

    clearAttachments.emplace_back(clearAttachment);
    clearRects.emplace_back(clearRect);
  }

  CHECK_ERR(commandBuffer->ClearAttachments(
      {static_cast<uint32_t>(clearAttachments.size()), clearAttachments.data(),
       static_cast<uint32_t>(clearRects.size()), clearRects.data()}));

  return Error::Success();
}

auto RenderTarget::GetHash() const -> uint64_t {
  if (dirty) {
    hash = HashRenderTarget(*this);
    dirty = false;
  }

  return hash;
}

auto State::GetHash() const -> uint64_t {
  if (dirty) {
    hash = StateKeyHash::Hash(StateKey(*this));
    dirty = false;
  }

  return hash;
}

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables, readability-function-cognitive-complexity, cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

} // namespace Graphics::RenderState
