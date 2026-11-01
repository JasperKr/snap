#pragma once
#include "Modules/console.hpp"
#include <cstddef>

#include <string>
#include <unordered_map>
#include <vulkan/vulkan_core.h>
namespace Graphics::Format {

auto StringToImageFormat(const std::string &format) -> VkFormat;
auto ImageFormatToString(VkFormat format) -> std::string_view;
auto ToString(VkFormat format, size_t arraySize = 1) -> std::string_view;
auto FromString(const std::string &format) -> VkFormat;
auto StringToArraySize(const std::string &format) -> size_t;
auto GetVec4Variant(VkFormat format) -> VkFormat;
auto GetBaseFormat(VkFormat format) -> VkFormat;

// Only allows base types, not vector or matrix types
auto BaseTypeToString(VkFormat format, uint8_t const *data) -> std::string;

// Allows base and vector types, but not matrix types.
// For matrix types you should just call this n times.
auto ToString(VkFormat format, uint8_t const *data) -> std::string;

auto GetChannelCount(VkFormat format) -> uint32_t;
constexpr auto GetSize(VkFormat format) -> uint32_t {
  static const std::unordered_map<VkFormat, uint32_t> FormatSizes = {
      // 1 channel
      {VK_FORMAT_R8_UNORM, 1},
      {VK_FORMAT_R8_SNORM, 1},
      {VK_FORMAT_R8_UINT, 1},
      {VK_FORMAT_R8_SINT, 1},
      {VK_FORMAT_R8_SRGB, 1},
      {VK_FORMAT_R16_UNORM, 2},
      {VK_FORMAT_R16_SNORM, 2},
      {VK_FORMAT_R16_UINT, 2},
      {VK_FORMAT_R16_SINT, 2},
      {VK_FORMAT_R16_SFLOAT, 2},
      {VK_FORMAT_R32_UINT, 4},
      {VK_FORMAT_R32_SINT, 4},
      {VK_FORMAT_R32_SFLOAT, 4},
      {VK_FORMAT_R64_UINT, 8},
      {VK_FORMAT_R64_SINT, 8},
      {VK_FORMAT_R64_SFLOAT, 8},
      // 2 channel
      {VK_FORMAT_R8G8_UNORM, 2},
      {VK_FORMAT_R8G8_SNORM, 2},
      {VK_FORMAT_R8G8_UINT, 2},
      {VK_FORMAT_R8G8_SINT, 2},
      {VK_FORMAT_R8G8_SRGB, 2},
      {VK_FORMAT_R16G16_UNORM, 4},
      {VK_FORMAT_R16G16_SNORM, 4},
      {VK_FORMAT_R16G16_UINT, 4},
      {VK_FORMAT_R16G16_SINT, 4},
      {VK_FORMAT_R16G16_SFLOAT, 4},
      {VK_FORMAT_R32G32_UINT, 8},
      {VK_FORMAT_R32G32_SINT, 8},
      {VK_FORMAT_R32G32_SFLOAT, 8},
      {VK_FORMAT_R64G64_UINT, 16},
      {VK_FORMAT_R64G64_SINT, 16},
      {VK_FORMAT_R64G64_SFLOAT, 16},
      // 3 channel
      {VK_FORMAT_R8G8B8_UNORM, 3},
      {VK_FORMAT_R8G8B8_SRGB, 3},
      {VK_FORMAT_B8G8R8_UNORM, 3},
      {VK_FORMAT_B8G8R8_SRGB, 3},
      {VK_FORMAT_R16G16B16_UNORM, 6},
      {VK_FORMAT_R16G16B16_SNORM, 6},
      {VK_FORMAT_R16G16B16_UINT, 6},
      {VK_FORMAT_R16G16B16_SINT, 6},
      {VK_FORMAT_R16G16B16_SFLOAT, 6},
      {VK_FORMAT_R32G32B32_UINT, 12},
      {VK_FORMAT_R32G32B32_SINT, 12},
      {VK_FORMAT_R32G32B32_SFLOAT, 12},
      {VK_FORMAT_R64G64B64_UINT, 24},
      {VK_FORMAT_R64G64B64_SINT, 24},
      {VK_FORMAT_R64G64B64_SFLOAT, 24},
      {VK_FORMAT_B10G11R11_UFLOAT_PACK32, 4},
      {VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, 4},
      // 4 channel
      {VK_FORMAT_R8G8B8A8_UNORM, 4},
      {VK_FORMAT_R8G8B8A8_SRGB, 4},
      {VK_FORMAT_R8G8B8A8_SNORM, 4},
      {VK_FORMAT_B8G8R8A8_UNORM, 4},
      {VK_FORMAT_B8G8R8A8_SRGB, 4},
      {VK_FORMAT_R8G8B8A8_UINT, 4},
      {VK_FORMAT_R8G8B8A8_SINT, 4},
      {VK_FORMAT_R16G16B16A16_UNORM, 8},
      {VK_FORMAT_R16G16B16A16_SNORM, 8},
      {VK_FORMAT_R16G16B16A16_UINT, 8},
      {VK_FORMAT_R16G16B16A16_SINT, 8},
      {VK_FORMAT_R16G16B16A16_SFLOAT, 8},
      {VK_FORMAT_R32G32B32A32_UINT, 16},
      {VK_FORMAT_R32G32B32A32_SINT, 16},
      {VK_FORMAT_R32G32B32A32_SFLOAT, 16},
      {VK_FORMAT_R64G64B64A64_UINT, 32},
      {VK_FORMAT_R64G64B64A64_SINT, 32},
      {VK_FORMAT_R64G64B64A64_SFLOAT, 32},
      // Depth / Stencil
      {VK_FORMAT_D16_UNORM, 2},
      {VK_FORMAT_D24_UNORM_S8_UINT, 4},
      {VK_FORMAT_X8_D24_UNORM_PACK32, 4},
      {VK_FORMAT_D32_SFLOAT, 4},
      {VK_FORMAT_D32_SFLOAT_S8_UINT, 5},
      // BC Compressed (block size)
      {VK_FORMAT_BC1_RGB_UNORM_BLOCK, 8},
      {VK_FORMAT_BC1_RGB_SRGB_BLOCK, 8},
      {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, 8},
      {VK_FORMAT_BC1_RGBA_SRGB_BLOCK, 8},
      {VK_FORMAT_BC2_UNORM_BLOCK, 16},
      {VK_FORMAT_BC2_SRGB_BLOCK, 16},
      {VK_FORMAT_BC3_UNORM_BLOCK, 16},
      {VK_FORMAT_BC3_SRGB_BLOCK, 16},
      {VK_FORMAT_BC4_UNORM_BLOCK, 8},
      {VK_FORMAT_BC4_SNORM_BLOCK, 8},
      {VK_FORMAT_BC5_UNORM_BLOCK, 16},
      {VK_FORMAT_BC5_SNORM_BLOCK, 16},
      {VK_FORMAT_BC6H_UFLOAT_BLOCK, 16},
      {VK_FORMAT_BC6H_SFLOAT_BLOCK, 16},
      {VK_FORMAT_BC7_UNORM_BLOCK, 16},
      {VK_FORMAT_BC7_SRGB_BLOCK, 16},
  };

  auto iter = FormatSizes.find(format);
  if (iter != FormatSizes.end()) {
    return iter->second;
  }

  auto formatStr = ToString(format, 1);
  PrintWarning("Unknown format {} for size retrieval.", formatStr);
  return 0;
}
constexpr auto IsCompressedFormat(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC2_SRGB_BLOCK:
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC3_SRGB_BLOCK:
  case VK_FORMAT_BC4_UNORM_BLOCK:
  case VK_FORMAT_BC4_SNORM_BLOCK:
  case VK_FORMAT_BC5_UNORM_BLOCK:
  case VK_FORMAT_BC5_SNORM_BLOCK:
  case VK_FORMAT_BC6H_UFLOAT_BLOCK:
  case VK_FORMAT_BC6H_SFLOAT_BLOCK:
  case VK_FORMAT_BC7_UNORM_BLOCK:
  case VK_FORMAT_BC7_SRGB_BLOCK:
    return true;
  default:
    return false;
  }
}

constexpr auto GetSize(VkFormat format, uint32_t width, uint32_t height)
    -> uint64_t {
  if (IsCompressedFormat(format)) {
    auto blockSize = GetSize(format);
    auto blockWidth = (width + 3) / 4;
    auto blockHeight = (height + 3) / 4;
    return static_cast<uint64_t>(blockWidth) * blockHeight * blockSize;
  }

  auto pixelSize = GetSize(format);
  return static_cast<uint64_t>(width) * height * pixelSize;
}

} // namespace Graphics::Format