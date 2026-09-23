#include "bloomManager.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "renderer.hpp"
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

auto BloomManager::Initialize(const Graphics::GraphicsContext &context)
    -> Error {
  // clang-format off
  DownsampleShader = CHECK_RES(Graphics::Shader::Create(context, "PostProcessing/Bloom/bloomDownsample", "Bloom downsample shader"));
  UpsampleShader =   CHECK_RES(Graphics::Shader::Create(context, "PostProcessing/Bloom/bloomUpsample",   "Bloom upsample shader"  ));
  ThresholdShader =  CHECK_RES(Graphics::Shader::Create(context, "PostProcessing/Bloom/bloomThreshold",  "Bloom threshold shader" ));
  DirtShader =       CHECK_RES(Graphics::Shader::Create(context, "PostProcessing/Bloom/bloomApplyDirt",  "Bloom dirt shader"      ));
  // clang-format on

  DirtTexture = CHECK_RES(Graphics::Texture::FromFile(
      context, "Graphics/Assets/lens_dirt.DDS",
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT));

  return {};
}

auto BloomManager::Deinitialize() -> void {
  DownsampleShader = nullptr;
  UpsampleShader = nullptr;
  ThresholdShader = nullptr;
  DirtShader = nullptr;
  DirtTexture = nullptr;
  DownsampleChainViews.clear();
}

// NOLINTNEXTLINE
auto BloomManager::ApplyBloom(const Graphics::GraphicsContext &context,
                              Camera &camera) -> Error {

  const auto &renderTargets = camera.GetRendertargets();
  auto &textures = camera.GetOwnedTextures();

  textures.BloomDownsampleChain =
      CHECK_RES(GlobalRenderTargetManager.GetRendertarget(
          context, renderTargets.BloomDownsampleChain));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                   .blendMode = Graphics::BlendmodeNone,
                   .texture = textures.BloomDownsampleChain,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);

  Graphics::RenderState::SetShader(ThresholdShader);
  const static Graphics::ResourceKey ThresholdKey = {"PushConstants",
                                                     "threshold"};

  CHECK_ERR(Graphics::UniformWriter::Send(ThresholdShader, ThresholdKey, 0.0F));

  const static Graphics::ResourceKey MainSceneKey = {"mainScene"};
  CHECK_ERR(ThresholdShader->Send(MainSceneKey, textures.TAA));

  CHECK_ERR(DrawFullScreen(context));

  Graphics::RenderState::SetShader(DownsampleShader);

  auto mipCount = textures.BloomDownsampleChain->levelCount;
  if (mipCount < 2) {
    return {};
  }

  auto maxDownsampleMip =
      std::min(mipCount - 1, static_cast<uint32_t>(8)); // NOLINT

  if (DownsampleChainViews.size() != mipCount) {
    DownsampleChainViews.resize(mipCount);
  }

  for (uint32_t mipLevel = 0; mipLevel < mipCount; ++mipLevel) {
    assert(mipLevel < DownsampleChainViews.size());
    auto view = DownsampleChainViews.at(mipLevel);
    if (view == nullptr ||
        view->imageMemory != textures.BloomDownsampleChain->imageMemory) {
      view = CHECK_RES(Graphics::Texture::Create(
          context, textures.BloomDownsampleChain.get(), VK_IMAGE_VIEW_TYPE_2D,
          VkImageSubresourceRange{
              .baseMipLevel = mipLevel,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          }));

      view->SetAnisotropy(0.0F);
      view->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                      VK_SAMPLER_MIPMAP_MODE_LINEAR);
      view->SetWrapmode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

      DownsampleChainViews.at(mipLevel) = view;
    }
  }

  for (uint32_t mipLevel = 1; mipLevel < maxDownsampleMip; ++mipLevel) {
    assert(mipLevel < DownsampleChainViews.size());
    auto &view = DownsampleChainViews.at(mipLevel);

    CHECK_ERR(Graphics::RenderState::SetRenderTargets(
        context, {Graphics::RenderState::RenderTarget{
                     .blendMode = Graphics::BlendmodeNone,
                     .texture = view,
                     .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                 }}));

    const static Graphics::ResourceKey DownsampleKey = {"mainScene"};
    CHECK_ERR(DownsampleShader->Send(DownsampleKey,
                                     DownsampleChainViews.at(mipLevel - 1)));

    const static Graphics::ResourceKey ViewportSizeKey = {"PushConstants",
                                                          "viewportSize"};
    CHECK_ERR(Graphics::UniformWriter::Send(
        DownsampleShader, ViewportSizeKey,
        Math::Uvec2(view->GetWidth(), view->GetHeight())));

    CHECK_ERR(DrawFullScreen(context));
  }

  Graphics::RenderState::SetShader(UpsampleShader);

  const static Graphics::ResourceKey IntensityKey = {"PushConstants",
                                                     "intensity"};
  const static Graphics::ResourceKey ViewportSizeKey = {"PushConstants",
                                                        "viewportSize"};

  // NOLINTNEXTLINE
  CHECK_ERR(Graphics::UniformWriter::Send(UpsampleShader, IntensityKey, 0.25F));
  CHECK_ERR(Graphics::UniformWriter::Send(
      UpsampleShader, ViewportSizeKey,
      Math::Uvec2(textures.BloomDownsampleChain->GetWidth(),
                  textures.BloomDownsampleChain->GetHeight())));

  for (auto mipLevel = maxDownsampleMip - 1; mipLevel > 1; --mipLevel) {
    assert(mipLevel < DownsampleChainViews.size());
    auto &view = DownsampleChainViews.at(mipLevel);

    CHECK_ERR(Graphics::RenderState::SetRenderTargets(
        context, {Graphics::RenderState::RenderTarget{
                     .blendMode = Graphics::BlendmodeAdditive,
                     .texture = DownsampleChainViews.at(mipLevel - 1),
                     .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                 }}));

    const static Graphics::ResourceKey DownsampleKey = {"mainScene"};
    CHECK_ERR(UpsampleShader->Send(DownsampleKey, view));

    CHECK_ERR(DrawFullScreen(context));
  }

  Graphics::RenderState::SetShader(DirtShader);
  const static Graphics::ResourceKey DirtTextureKey = {"mainScene"};
  CHECK_ERR(DirtShader->Send(DirtTextureKey, DirtTexture));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                   .blendMode = Graphics::BlendmodeMultiply,
                   .texture = DownsampleChainViews.at(1),
                   .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
               }}));

  CHECK_ERR(DrawFullScreen(context));

  textures.BloomFinal = CHECK_RES(GlobalRenderTargetManager.GetRendertarget(
      context, renderTargets.IncomingLight));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                   .blendMode = Graphics::BlendmodeAdditive,
                   .texture = textures.BloomFinal,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
               }}));

  Graphics::RenderState::SetShader({});
  CHECK_ERR(Graphics::Draw(context, *textures.TAA));
  CHECK_ERR(Graphics::Draw(context, *DownsampleChainViews.at(1)));

  CHECK_ERR(GlobalRenderTargetManager.ReleaseRendertarget(
      textures.BloomDownsampleChain));

  return {};
}
} // namespace Engine::Renderer