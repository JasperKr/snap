#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace Image {

enum class DDS_FLAGS {
  ALPHAPIXELS = 0x1,
  ALPHA = 0x2,
  FOURCC = 0x4,
  RGB = 0x40,
  YUV = 0x200,
  LUMINANCE = 0x20000,
};

enum class DDS_HEADER_FLAGS : uint32_t {
  CAPS = 0x1,
  HEIGHT = 0x2,
  WIDTH = 0x4,
  PITCH = 0x8,
  PIXELFORMAT = 0x1000,
  MIPMAPCOUNT = 0x20000,
  LINEARSIZE = 0x80000,
  DEPTH = 0x800000,
};

enum class DDS_CAPS : uint32_t {
  COMPLEX = 0x8,
  MIPMAP = 0x400000,
  TEXTURE = 0x1000,
};

struct DDS_HEADER_DXT10 {
  uint32_t dxgiFormat;        // DXGI_FORMAT enum (e.g. 98 = BC7_UNORM)
  uint32_t resourceDimension; // e.g. 3 = TEXTURE2D
  uint32_t miscFlag;          // e.g. 0 or 0x4 for cubemaps
  uint32_t arraySize;         // Usually 1 unless texture array
  uint32_t miscFlags2;        // Reserved (set to 0)
};

struct DDS_PIXELFORMAT {
  uint32_t size;
  uint32_t flags;
  uint32_t fourCC;
  uint32_t rgbBitCount;
  uint32_t rBitMask;
  uint32_t gBitMask;
  uint32_t bBitMask;
  uint32_t aBitMask;
};

struct DDS_HEADER {
  uint32_t size;
  uint32_t flags;
  uint32_t height;
  uint32_t width;
  uint32_t pitchOrLinearSize;
  uint32_t depth;
  uint32_t mipMapCount;
  uint32_t reserved[11]; // NOLINT
  DDS_PIXELFORMAT ddspf;
  uint32_t caps;
  uint32_t caps2;
  uint32_t caps3;
  uint32_t caps4;
  uint32_t reserved2;
};

inline auto DXgiDDSFormatToVkFormat(uint32_t dxgiFormat) {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
  switch (dxgiFormat) {
  case 71: // DXGI_FORMAT_BC1_UNORM
  {
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  }
  case 72: // DXGI_FORMAT_BC1_UNORM_SRGB
  {
    return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
  }
  case 74: // DXGI_FORMAT_BC2_UNORM
  {
    return VK_FORMAT_BC2_UNORM_BLOCK;
  }
  case 75: // DXGI_FORMAT_BC2_UNORM_SRGB
  {
    return VK_FORMAT_BC2_SRGB_BLOCK;
  }
  case 77: // DXGI_FORMAT_BC3_UNORM
  {
    return VK_FORMAT_BC3_UNORM_BLOCK;
  }
  case 78: // DXGI_FORMAT_BC3_UNORM_SRGB
  {
    return VK_FORMAT_BC3_SRGB_BLOCK;
  }
  case 80: // DXGI_FORMAT_BC4_UNORM
  {
    return VK_FORMAT_BC4_UNORM_BLOCK;
  }
  case 81: // DXGI_FORMAT_BC4_SNORM
  {
    return VK_FORMAT_BC4_SNORM_BLOCK;
  }
  case 83: // DXGI_FORMAT_BC5_UNORM
  {
    return VK_FORMAT_BC5_UNORM_BLOCK;
  }
  case 84: // DXGI_FORMAT_BC5_SNORM
  {
    return VK_FORMAT_BC5_SNORM_BLOCK;
  }
  case 95: // DXGI_FORMAT_BC6H_UF16
  {
    return VK_FORMAT_BC6H_UFLOAT_BLOCK;
  }
  case 96: // DXGI_FORMAT_BC6H_SF16
  {
    return VK_FORMAT_BC6H_SFLOAT_BLOCK;
  }
  case 98: // DXGI_FORMAT_BC7_UNORM
  {
    return VK_FORMAT_BC7_UNORM_BLOCK;
  }
  case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
  {
    return VK_FORMAT_BC7_SRGB_BLOCK;
  }
  default:
    return VK_FORMAT_UNDEFINED;
  }
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
}

constexpr auto MakeFourCC(char char0, char char1, char char2, char char3) -> uint32_t {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
  return (uint32_t(uint8_t(char0))) | (uint32_t(uint8_t(char1)) << 8UL) | (uint32_t(uint8_t(char2)) << 16UL) | (uint32_t(uint8_t(char3)) << 24UL);
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
}

inline auto DDSFourCCToVkFormat(uint32_t fourCC) -> VkFormat {
  switch (fourCC) {
  case MakeFourCC('D', 'X', 'T', '1'):
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  case MakeFourCC('D', 'X', 'T', '3'):
    return VK_FORMAT_BC2_UNORM_BLOCK;
  case MakeFourCC('D', 'X', 'T', '5'):
    return VK_FORMAT_BC3_UNORM_BLOCK;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

} // namespace Image