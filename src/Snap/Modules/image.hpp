#pragma once
#include "vulkan/vulkan_core.h"
#include <span>
#include <string_view>

namespace Image {

// Returns total texel count for ALL mip levels, for ONE layer only.
// Caller must multiply by arrayLayers or 6 for cubemaps if needed.
constexpr auto GetTexelCount(const VkExtent2D &extent, uint32_t mipmapCount)
    -> uint64_t {
  uint64_t totalTexels = 0;
  for (uint64_t mip = 0; mip < mipmapCount; mip++) {
    uint64_t mipWidth = (std::max)(1U, extent.width >> mip);
    uint64_t mipHeight = (std::max)(1U, extent.height >> mip);

    totalTexels += mipWidth * mipHeight;
  }

  return totalTexels;
}

// Returns total texel count for ALL mip levels, for ONE layer only.
// Caller must multiply by arrayLayers or 6 for cubemaps if needed.
constexpr auto GetTexelCount(const VkExtent3D &extent, uint32_t mipmapCount)
    -> uint64_t {
  uint64_t totalTexels = 0;
  for (uint64_t mip = 0; mip < mipmapCount; mip++) {
    uint64_t mipWidth = (std::max)(1U, extent.width >> mip);
    uint64_t mipHeight = (std::max)(1U, extent.height >> mip);
    uint64_t mipDepth = (std::max)(1U, extent.depth >> mip);

    totalTexels += mipWidth * mipHeight * mipDepth;
  }

  return totalTexels;
}

constexpr auto GetMipmapCount(VkExtent2D extent) -> uint32_t {
  uint32_t mipLevels = 1;
  while (extent.width > 1U || extent.height > 1U) {
    mipLevels++;
    extent.width = std::max(1U, extent.width >> 1U);
    extent.height = std::max(1U, extent.height >> 1U);
  }
  return mipLevels;
}

constexpr auto GetMipmapCount(VkExtent3D extent) -> uint32_t {
  uint32_t mipLevels = 1;
  while (extent.width > 1U || extent.height > 1U || extent.depth > 1U) {
    mipLevels++;
    extent.width = std::max(1U, extent.width >> 1U);
    extent.height = std::max(1U, extent.height >> 1U);
    extent.depth = std::max(1U, extent.depth >> 1U);
  }
  return mipLevels;
}

constexpr auto GetMipmapCount(uint32_t width, uint32_t height) -> uint32_t {
  return GetMipmapCount(VkExtent2D{width, height});
}

constexpr auto GetMipmapCount(uint32_t width, uint32_t height, uint32_t depth)
    -> uint32_t {
  return GetMipmapCount(VkExtent3D{width, height, depth});
}

auto IsDepthTexture(VkFormat format) -> bool;

auto IsStencilTexture(VkFormat format) -> bool;

auto IsDepthOrStencilTexture(VkFormat format) -> bool;

auto GetTextureAspectFlags(VkFormat format) -> VkImageAspectFlags;

auto GetDimensions(const VkExtent3D &extent, uint32_t mipLevel) -> VkExtent3D;

auto GetDimensions(const VkExtent2D &extent, uint32_t mipLevel) -> VkExtent2D;

auto IsCompressedTexture(VkFormat format) -> bool;

auto IsDDS(const std::span<const uint8_t> &data) -> bool;

auto IsNormalisedFormat(VkFormat format) -> bool;
auto IsIntegerFormat(VkFormat format) -> bool;

auto ImageLayoutToString(VkImageLayout layout) -> std::string_view;

} // namespace Image