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

struct SamplerDescHash {
  auto operator()(const SamplerDescription &desc) const -> size_t;
};

extern std::unordered_map<SamplerDescription, VkDeviceSize, SamplerDescHash> SamplerCache; // NOLINT

auto GetOrCreateSampler(const struct GraphicsContext &context, const SamplerDescription &description) -> Result<VkDeviceSize>;

auto DestroySamplers(const struct GraphicsContext &context) -> void;

}; // namespace Graphics