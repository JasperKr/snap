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
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
RenderTargetManager GlobalRenderTargetManager;

auto RendertargetDescriptor::Score(const RendertargetDescriptor &other) const -> int {
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

auto RendertargetDescriptor::operator==(const RendertargetDescriptor &other) const -> bool {
  return size == other.size && GetMipmapCount() == other.GetMipmapCount() && arrayLayers == other.arrayLayers && format == other.format &&
         minFilter == other.minFilter && magFilter == other.magFilter && mipFilter == other.mipFilter && addressModeU == other.addressModeU &&
         addressModeV == other.addressModeV && addressModeW == other.addressModeW && borderColor == other.borderColor && usage == other.usage;
}

auto RendertargetDescriptor::operator!=(const RendertargetDescriptor &other) const -> bool { return !(*this == other); }

auto RendertargetDescriptor::GetMipmapCount() const -> uint32_t {
  if (mipmapCount > 0) {
    return mipmapCount;
  }

  if (!requiresMipmaps) {
    return 1;
  }

  return Image::GetMipmapCount(size.x, size.y);
}

auto RendertargetDescriptor::ToString() const -> std::string {
  return std::format("Size: {}x{}, mipmaps? {}, levelCount: {}, layerCount: {}, format: {}", size.x, size.y, requiresMipmaps ? "yes" : "no", GetMipmapCount(),
                     arrayLayers, Graphics::Format::ImageFormatToString(format));
}

auto RendertargetDescriptorHash::operator()(const RendertargetDescriptor &desc) const -> size_t {
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

inline auto ImageSubresourceRangeEquality(VkImageSubresourceRange first, VkImageSubresourceRange second) -> bool {
  return first.levelCount == second.levelCount && first.layerCount == second.layerCount && first.baseArrayLayer == second.baseArrayLayer &&
         first.baseMipLevel == second.baseMipLevel;
}

inline auto ReconfigureTexture(const RendertargetDescriptor &descriptor, const Ref<::Graphics::Texture> &texture) -> void {
  texture->SetFilter(descriptor.minFilter, descriptor.magFilter, descriptor.mipFilter);
  texture->SetWrapmode(descriptor.addressModeU, descriptor.addressModeV, descriptor.addressModeW);
  texture->SetBorderColor(descriptor.borderColor);
}

auto RendertargetHandle::GetTexture() -> Ref<Graphics::Texture> {
  for (auto &view : views) {
    if (view->subresourceRange.levelCount != descriptor.GetMipmapCount()) {
      continue;
    }

    if (view->subresourceRange.layerCount != descriptor.arrayLayers) {
      continue;
    }

    return view;
  }

  assert(false);
}

auto RendertargetHandle::GetTexture(const Graphics::GraphicsContext &context, VkImageSubresourceRange range) -> Result<Ref<Graphics::Texture>> {
  Ref<Graphics::Texture> view;

  for (auto &existingView : views) {
    if (!ImageSubresourceRangeEquality(existingView->subresourceRange, range)) {
      continue;
    }

    view = existingView;
  }

  if (view == nullptr) {
    view = CHECK_RES(Graphics::Texture::Create(context, texture.get(), VK_IMAGE_VIEW_TYPE_2D, range));
    views.emplace_back(view);
  }

  ReconfigureTexture(descriptor, view);

  return view;
}

auto RenderTargetManager::GetRendertarget(const struct ::Graphics::GraphicsContext &context, const RendertargetDescriptor &descriptor)
    -> Result<RendertargetHandle> {
  // First try to find an exact match.
  for (auto &entry : Rendertargets) {
    if (entry.originalDescriptor == descriptor && !entry.inUse) {
      entry.inUse = true;
      entry.lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;
      entry.handle.descriptor = descriptor;
      return entry.handle;
    }
  }

  // If no exact match, find the best candidate that meets the requirements.
  int bestScore = -1;
  RendertargetEntry *bestEntry = nullptr;
  for (auto &rendertarget : Rendertargets) {
    int score = rendertarget.originalDescriptor.Score(descriptor);
    if (score > bestScore && !rendertarget.inUse) {
      bestScore = score;
      bestEntry = &rendertarget;
    }
  }

  if (bestEntry != nullptr) {
    assert(!bestEntry->inUse);

    bestEntry->handle.descriptor = descriptor;
    bestEntry->inUse = true;
    bestEntry->lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;

    bool foundViewMatchingDescriptor = false;

    for (auto &view : bestEntry->handle.views) {
      if (view->subresourceRange.levelCount != descriptor.GetMipmapCount()) {
        continue;
      }

      if (view->subresourceRange.layerCount != descriptor.arrayLayers) {
        continue;
      }

      foundViewMatchingDescriptor = true;
    }

    if (!foundViewMatchingDescriptor) {
      auto view = CHECK_RES(Graphics::Texture::Create(context, bestEntry->handle.texture.get(), VK_IMAGE_VIEW_TYPE_2D,
                                                      VkImageSubresourceRange{
                                                          .baseMipLevel = 0,
                                                          .levelCount = descriptor.GetMipmapCount(),
                                                          .baseArrayLayer = 0,
                                                          .layerCount = static_cast<uint32_t>(descriptor.arrayLayers),
                                                      }));
      bestEntry->handle.views.emplace_back(view);
    }

    return bestEntry->handle;
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
      message += std::format("Texture: {}, Descriptor: {}\n", (void *)entry.handle.texture->imageMemory->image, entry.originalDescriptor.ToString());
    }

    PrintError("RenderTargetManager: {}", message);

    return Error::Unexpectedf("Exceeded maximum number of rendertargets ({}).", MaxRendertargets);
  }

  auto info = ::Graphics::TextureCreationInfo{
      .size = {descriptor.size.x, descriptor.size.y, 1},
      .arrayLayers = static_cast<uint32_t>(descriptor.arrayLayers),
      .format = descriptor.format,
      .usage = descriptor.usage,
      .mipmapCount = static_cast<int>(descriptor.GetMipmapCount()),
      .debugName = std::format("Rendertarget / {} / {}x{} #{}", Graphics::Format::ImageFormatToString(descriptor.format), std::to_string(descriptor.size.x),
                               std::to_string(descriptor.size.y), Rendertargets.size()),
      .textureType = descriptor.arrayLayers == 1 ? ::Graphics::TextureType::DEFAULT : ::Graphics::TextureType::ARRAY,
  };

  auto texture = CHECK_RES(::Graphics::Texture::Create(context, info));
  Rendertargets.push_back({
      .inUse = true,
      .lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame,
      .handle = RendertargetHandle(descriptor, texture),
      .originalDescriptor = descriptor,
  });

  auto view = CHECK_RES(Graphics::Texture::Create(context, texture.get(), VK_IMAGE_VIEW_TYPE_2D,
                                                  VkImageSubresourceRange{
                                                      .baseMipLevel = 0,
                                                      .levelCount = descriptor.GetMipmapCount(),
                                                      .baseArrayLayer = 0,
                                                      .layerCount = static_cast<uint32_t>(descriptor.arrayLayers),
                                                  }));
  Rendertargets.back().handle.views.emplace_back(view);

  return Rendertargets.back().handle;
}

auto RenderTargetManager::ReleaseRendertarget(const Ref<::Graphics::Texture> &texture) -> Result<bool> {
  if (texture == nullptr) {
    return false;
  }

  size_t count = 0;
  for (auto &entry : Rendertargets) {
    if (entry.handle.texture->imageMemory->image == texture->imageMemory->image) {
      entry.inUse = false;
      entry.lastUsedFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;
      count++;
    }
  }

  if (count > 1) {
    return Error::Unexpected("RenderTargetManager: Released more than one rendertarget texture");
  }

  return count == 1;
}

auto RenderTargetManager::ReleaseRendertargets(const std::vector<Ref<::Graphics::Texture>> &textures) -> Result<bool> {
  bool allSuccess = true;

  for (const auto &texture : textures) {
    allSuccess = allSuccess && CHECK_RES(ReleaseRendertarget(texture));
  }

  return allSuccess;
}

// Clean up old rendertargets that haven't been used for a while.
// Optionally force all unused rendertargets to be evicted prematurely
auto RenderTargetManager::Cleanup(bool evictAll) -> void {
  auto currentFrame = Graphics::GetCurrentGraphicsContext()->currentFrame;
  Utils::UnorderedErase(Rendertargets, [currentFrame, evictAll](RendertargetEntry &entry) -> bool {
    bool expired = (currentFrame - entry.lastUsedFrame) > RendertargetLifetime;
    bool clean = !entry.inUse && (expired || evictAll);
    if (clean) {
      entry.handle.views.clear();
      entry.handle.texture = nullptr;
    }

    return clean;
  });
}

auto RenderTargetManager::Update() -> void { Cleanup(); }

auto RenderTargetManager::Deinitialize() -> void { Rendertargets.clear(); }

} // namespace Engine::Renderer