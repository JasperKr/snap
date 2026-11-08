#pragma once

#include "Graphics/texture.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

// Max render targets, a safety limit to prevent a scenario where the user forgot to call ReleaseRendertarget.
static constexpr size_t MaxRendertargets = 128;
static constexpr size_t RendertargetLifetime = 32; // 32 frames.

struct RendertargetDescriptor {
  Math::Uvec2 size{};
  bool requiresMipmaps = false;
  bool persistent = false;

  // -1 means auto-calculate based on size and requiresMipmaps
  int32_t mipmapCount = -1;
  int32_t arrayLayers = 1;
  VkFormat format = VK_FORMAT_UNDEFINED;
  VkFilter minFilter = VK_FILTER_NEAREST;
  VkFilter magFilter = VK_FILTER_NEAREST;
  VkSamplerMipmapMode mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;

  // Score using the rendertarget of this descriptor as a candidate for the other descriptor's requirements.
  [[nodiscard]] auto Score(const RendertargetDescriptor &other) const -> int;
  [[nodiscard]] auto GetMipmapCount() const -> uint32_t;

  auto operator==(const RendertargetDescriptor &other) const -> bool;
  auto operator!=(const RendertargetDescriptor &other) const -> bool;
};

struct RendertargetDescriptorHash {
  auto operator()(const RendertargetDescriptor &desc) const -> size_t;
};

struct RenderTargetManager {
  auto GetRendertarget(const struct ::Graphics::GraphicsContext &context, const RendertargetDescriptor &descriptor) -> Result<Ref<::Graphics::Texture>>;
  auto ReleaseRendertarget(const Ref<::Graphics::Texture> &texture) -> Error;
  auto ReleaseRendertargets(const std::vector<Ref<::Graphics::Texture>> &textures) -> Error;

  [[nodiscard]] auto GetRendertargetCount() const -> size_t { return Rendertargets.size(); }
  auto Update() -> void;

  auto Deinitialize() -> void;

private:
  struct RendertargetEntry {
    RendertargetDescriptor descriptor;
    bool inUse = false;
    Ref<::Graphics::Texture> texture;
    uint64_t lastUsedFrame = 0;
  };

  std::vector<RendertargetEntry> Rendertargets;

  static auto ReconfigureTexture(const RendertargetDescriptor &descriptor, const Ref<::Graphics::Texture> &texture) -> void;

  auto Cleanup(bool evictAll = false) -> void;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern RenderTargetManager GlobalRenderTargetManager;

} // namespace Engine::Renderer