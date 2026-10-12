#pragma once

#include "Modules/error.hpp"
#include "vulkan/vulkan_core.h"
#include <unordered_map>

namespace Graphics {

struct SamplerDescription {
  VkFilter magFilter = VK_FILTER_NEAREST;
  VkFilter minFilter = VK_FILTER_NEAREST;
  VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  float mipLodBias{};
  bool anisotropyEnable{};
  float maxAnisotropy{};
  bool compareEnable{};
  VkCompareOp compareOp = VK_COMPARE_OP_NEVER;
  float minLod{};
  float maxLod = VK_LOD_CLAMP_NONE;
  VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

  auto operator==(const SamplerDescription &other) const -> bool = default;
};

auto inline Hash(size_t seed, size_t value) -> size_t {
  constexpr size_t offset = 0x9e3779b9;
  constexpr size_t lshift = 6;
  constexpr size_t rshift = 2;

  return seed ^ (value + offset + (seed << lshift) + (seed >> rshift));
}

struct SamplerDescHash {
  auto operator()(const SamplerDescription &desc) const -> size_t {
    size_t hash = 0;

    auto Hasher = [&](size_t value) -> void { hash = Hash(hash, value); };

    Hasher(desc.magFilter);
    Hasher(desc.minFilter);
    Hasher(desc.mipmapMode);
    Hasher(desc.addressModeU);
    Hasher(desc.addressModeV);
    Hasher(desc.addressModeW);
    Hasher(std::hash<float>()(desc.mipLodBias));
    Hasher(static_cast<size_t>(desc.anisotropyEnable));
    Hasher(std::hash<float>()(desc.maxAnisotropy));
    Hasher(static_cast<size_t>(desc.compareEnable));
    Hasher(desc.compareOp);
    Hasher(std::hash<float>()(desc.minLod));
    Hasher(std::hash<float>()(desc.maxLod));
    Hasher(desc.borderColor);

    return hash;
  }
};

extern std::unordered_map<SamplerDescription, VkDeviceSize, SamplerDescHash>
    SamplerCache; // NOLINT

auto GetOrCreateSampler(const struct GraphicsContext &context,
                        const SamplerDescription &description)
    -> Result<VkDeviceSize>;

auto DestroySamplers(const struct GraphicsContext &context) -> void;

}; // namespace Graphics