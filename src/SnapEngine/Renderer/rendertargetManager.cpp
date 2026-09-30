#include "rendertargetManager.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/object.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <string>

namespace Engine::Renderer {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
RenderTargetManager GlobalRenderTargetManager;

auto RendertargetDescriptor::Score(const RendertargetDescriptor &other) const
    -> int {
  uint32_t score = 0;
  constexpr auto SizeMatchScore = 10;
  constexpr auto SizeDiffPixelCount = 100;
  constexpr auto MipmapMatchScore = 10;
  constexpr auto UsageMatchScore = 5;
  constexpr auto ArrayLayerMatchScore = 5;
  constexpr auto ArrayLayerDiffCount = 1;

  if ((usage & other.usage) == 0) {
    // If the usage does not match, this rendertarget cannot be used.
    return -1;
  }

  if (usage == other.usage) {
    score += UsageMatchScore;
  }

  // if (size.x < other.size.x || size.y < other.size.y) {
  //   // This rendertarget is too small for the requirements.
  //   return -1;
  // }
  // if (size.x == other.size.x && size.y == other.size.y) {
  //   score += SizeMatchScore;
  // } else {
  //   // The rendertarget is larger than the requirements, deduct points based on difference.
  //   auto diff = std::max(size.x - other.size.x, size.y - other.size.y);
  //   score += std::max(1U, SizeMatchScore - (diff / SizeDiffPixelCount));
  // }

  // ^ For now we force a perfect size match until we can return a texture view with a different size than the underlying texture
  if (size != other.size) {
    return -1;
  }

  if (arrayLayers < other.arrayLayers) {
    // This rendertarget doesn't have enough array layers for the requirements.
    return -1;
  }

  if (arrayLayers != other.arrayLayers) {
    // The rendertarget has more array layers than the requirements, deduct points based on difference.
    auto diff = arrayLayers - other.arrayLayers;

    // If diff > ArrayLayerMatchScore, we start deducting score.
    score += ArrayLayerMatchScore - (diff * ArrayLayerDiffCount);
  }

  if (GetMipmapCount() < other.GetMipmapCount()) {
    // This rendertarget doesn't have enough mip levels for the requirements.
    return -1;
  }
  if (GetMipmapCount() >= other.GetMipmapCount()) {
    auto distance = static_cast<int>(GetMipmapCount() - other.GetMipmapCount());
    score += std::max(1, MipmapMatchScore - distance);
  }

  if (format != other.format) {
    return -1;
  }

  return static_cast<int>(score);
};

auto RendertargetDescriptor::operator==(
    const RendertargetDescriptor &other) const -> bool {
  return size == other.size && GetMipmapCount() == other.GetMipmapCount() &&
         arrayLayers == other.arrayLayers && format == other.format &&
         minFilter == other.minFilter && magFilter == other.magFilter &&
         mipFilter == other.mipFilter && addressModeU == other.addressModeU &&
         addressModeV == other.addressModeV &&
         addressModeW == other.addressModeW &&
         borderColor == other.borderColor && usage == other.usage;
}

auto RendertargetDescriptor::operator!=(
    const RendertargetDescriptor &other) const -> bool {
  return !(*this == other);
}

auto RendertargetDescriptor::GetMipmapCount() const -> uint32_t {
  if (mipmapCount > 0) {
    return mipmapCount;
  }

  if (!requiresMipmaps) {
    return 1;
  }

  return Image::GetMipmapCount(size.x, size.y);
}

auto RendertargetDescriptorHash::operator()(
    const RendertargetDescriptor &desc) const -> size_t {
  Hash::Hasher hasher;
  hasher.Add(desc.size.x);
  hasher.Add(desc.size.y);
  hasher.Add(desc.GetMipmapCount());
  hasher.Add(desc.format);
  hasher.Add(desc.minFilter);
  hasher.Add(desc.magFilter);
  hasher.Add(desc.mipFilter);
  hasher.Add(desc.addressModeU);
  hasher.Add(desc.addressModeV);
  hasher.Add(desc.addressModeW);
  hasher.Add(desc.borderColor);
  hasher.Add(desc.usage);
  hasher.Add(desc.arrayLayers);
  return hasher.Get();
}

auto RenderTargetManager::GetRendertarget(
    const struct ::Graphics::GraphicsContext &context,
    const RendertargetDescriptor &descriptor)
    -> Result<Ref<::Graphics::Texture>> {
  // First try to find an exact match.
  for (auto &entry : Rendertargets) {
    if (entry.descriptor == descriptor && !entry.inUse) {
      entry.inUse = true;
      entry.lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;
      return entry.texture;
    }
  }

  // If no exact match, find the best candidate that meets the requirements.
  int bestScore = -1;
  int bestTextureIndex = -1;
  Ref<::Graphics::Texture> bestTexture;
  for (auto i = 0; i < Rendertargets.size(); i++) {
    const auto &entry = Rendertargets[i];

    int score = entry.descriptor.Score(descriptor);
    if (score > bestScore && !entry.inUse) {
      bestScore = score;
      bestTexture = entry.texture;
      bestTextureIndex = i;
    }
  }

  if (bestTextureIndex != -1) {
    ReconfigureTexture(descriptor, bestTexture);
    auto &entry = Rendertargets[bestTextureIndex];
    assert(!entry.inUse);

    entry.descriptor = descriptor;
    entry.inUse = true;
    entry.lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;

    return bestTexture;
  }

  // This might look a bit strange;
  if (Rendertargets.size() >= MaxRendertargets) {
    Cleanup(); // Cleanup old rendertargets

    if (Rendertargets.size() >= MaxRendertargets) {
      Cleanup(true); // Force cleanup all unused rendertargets
    }

    // We do a normal cleanup first, if that's not enough, try to evict anything we can
    // If this still fails we raise an error.
  }

  if (Rendertargets.size() >= MaxRendertargets) {
    std::string message = "Currently allocated:\n";
    for (const auto &entry : Rendertargets) {
      message += std::format(
          "- {}x{}, format: {}, usage: {}, mip levels: {}, filters: {} / {}\n",
          entry.descriptor.size.x, entry.descriptor.size.y,
          Graphics::Format::ImageFormatToString(entry.descriptor.format),
          (int)entry.descriptor.usage, entry.descriptor.GetMipmapCount(),
          (int)entry.descriptor.minFilter, (int)entry.descriptor.magFilter);
    }

    PrintError("RenderTargetManager: {}", message);

    return Error::Unexpectedf("Exceeded maximum number of rendertargets ({}).",
                              MaxRendertargets);
  }

  auto info = ::Graphics::TextureCreationInfo{
      .size = {descriptor.size.x, descriptor.size.y, 1},
      .arrayLayers = static_cast<uint32_t>(descriptor.arrayLayers),
      .format = descriptor.format,
      .usage = descriptor.usage,
      .mipmapCount = static_cast<int>(descriptor.GetMipmapCount()),
      .debugName =
          std::format("Rendertarget / {} / {}x{} #{}",
                      Graphics::Format::ImageFormatToString(descriptor.format),
                      std::to_string(descriptor.size.x),
                      std::to_string(descriptor.size.y), Rendertargets.size()),
      .textureType = descriptor.arrayLayers == 1
                         ? ::Graphics::TextureType::DEFAULT
                         : ::Graphics::TextureType::ARRAY,
  };

  auto texture = CHECK_RES(::Graphics::Texture::Create(context, info));

  Rendertargets.push_back({
      .descriptor = descriptor,
      .inUse = true,
      .texture = texture,
      .lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame,
  });

  return texture;
}

auto RenderTargetManager::ReleaseRendertarget(
    const Ref<::Graphics::Texture> &texture) -> Error {
  if (texture == nullptr) {
    return {};
  }

  size_t count = 0;
  for (auto &entry : Rendertargets) {
    if (entry.texture == texture) {
      entry.inUse = false;
      entry.lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;
      count++;
    }
  }

  if (count > 1) {
    return Error::Create(
        "RenderTargetManager: Released more than one rendertarget texture");
  }

  return {};
}

auto RenderTargetManager::ReleaseRendertargets(
    const std::vector<Ref<::Graphics::Texture>> &textures) -> Error {
  for (const auto &texture : textures) {
    CHECK_ERR(ReleaseRendertarget(texture));
  }

  return {};
}

auto RenderTargetManager::ReconfigureTexture(
    const RendertargetDescriptor &descriptor,
    const Ref<::Graphics::Texture> &texture) -> void {
  texture->SetFilter(descriptor.minFilter, descriptor.magFilter,
                     descriptor.mipFilter);
  texture->SetWrapmode(descriptor.addressModeU, descriptor.addressModeV,
                       descriptor.addressModeW);
  texture->SetBorderColor(descriptor.borderColor);
}

// Clean up old rendertargets that haven't been used for a while.
// Optionally force all unused rendertargets to be evicted prematurely
auto RenderTargetManager::Cleanup(bool evictAll) -> void {
  auto currentFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;
  Utils::UnorderedErase(
      Rendertargets,
      [currentFrame, evictAll](RendertargetEntry &entry) -> bool {
        bool expired =
            (currentFrame - entry.lastUsedFrame) > RendertargetLifetime;
        bool clean = !entry.inUse && (expired || evictAll);
        if (clean) {
          entry.texture = nullptr;
        }

        return clean;
      });
}

auto RenderTargetManager::Update() -> void { Cleanup(); }

auto RenderTargetManager::Deinitialize() -> void { Rendertargets.clear(); }

} // namespace Engine::Renderer