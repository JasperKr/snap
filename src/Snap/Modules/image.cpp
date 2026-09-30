#include "Modules/image.hpp"
#include "Modules/dds.hpp"
#include <algorithm>
#include <cstring>
#include <span>
#include <vulkan/vulkan_core.h>

namespace Image {

auto IsDepthTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

auto IsStencilTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

auto IsDepthOrStencilTexture(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
  case VK_FORMAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

auto GetTextureAspectFlags(VkFormat format) -> VkImageAspectFlags {
  VkImageAspectFlags aspectFlags = 0;

  if (IsDepthTexture(format)) {
    aspectFlags |= static_cast<uint32_t>(VK_IMAGE_ASPECT_DEPTH_BIT);
  }

  if (IsStencilTexture(format)) {
    aspectFlags |= static_cast<uint32_t>(VK_IMAGE_ASPECT_STENCIL_BIT);
  }

  if (aspectFlags == 0) {
    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  return aspectFlags;
}

auto GetDimensions(const VkExtent3D &extent, uint32_t mipLevel) -> VkExtent3D {
  return VkExtent3D{
      .width = std::max(1U, extent.width >> mipLevel),
      .height = std::max(1U, extent.height >> mipLevel),
      .depth = std::max(1U, extent.depth >> mipLevel),
  };
}

auto GetDimensions(const VkExtent2D &extent, uint32_t mipLevel) -> VkExtent2D {
  return VkExtent2D{
      .width = std::max(1U, extent.width >> mipLevel),
      .height = std::max(1U, extent.height >> mipLevel),
  };
}

auto IsCompressedTexture(VkFormat format) -> bool {
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

constexpr auto DDS_MAGIC = MakeFourCC('D', 'D', 'S', ' ');

auto IsDDS(const std::span<const uint8_t> &data) -> bool {
  if (data.size() < 4) {
    return false;
  }

  return (std::memcmp(data.data(), &DDS_MAGIC, 4) == 0);
}

/*
auto IsNormalisedFormat(VkFormat format) -> bool;
auto IsIntegerFormat(VkFormat format) -> bool;
*/

auto IsNormalisedFormat(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16A16_UNORM:
    return true;
  default:
    return false;
  }
}

auto IsIntegerFormat(VkFormat format) -> bool {
  switch (format) {
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_B8G8R8_UINT:
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_B8G8R8A8_UINT:
  case VK_FORMAT_A2B10G10R10_UINT_PACK32:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_B8G8R8_SINT:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_B8G8R8A8_SINT:
  case VK_FORMAT_A2B10G10R10_SINT_PACK32:
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32A32_SINT:
    return true;
  default:
    return false;
  }
}

auto ImageLayoutToString(VkImageLayout layout) -> std::string_view {
  // clang-format off
  switch (layout) {
  case VK_IMAGE_LAYOUT_UNDEFINED: return "Undefined";
  case VK_IMAGE_LAYOUT_GENERAL: return "General";
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "Color attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "Depth stencil attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "Depth stencil read only optimal";
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "Shader read only optimal";
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "Transfer src optimal";
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "Transfer dst optimal";
  case VK_IMAGE_LAYOUT_PREINITIALIZED: return "Preinitialized";
  case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return "Depth read only stencil attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL: return "Depth attachment stencil read only optimal";
  case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return "Depth attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return "Depth read only optimal";
  case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return "Stencil attachment optimal";
  case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return "Stencil read only optimal";
  case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return "Read only optimal";
  case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return "Attachment optimal";
  case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ: return "Rendering local read";
  case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "Present src khr";
  case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR: return "Video decode dst khr";
  case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR: return "Video decode src khr";
  case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR: return "Video decode dpb khr";
  case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR: return "Shared present khr";
  case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT: return "Fragment density map optimal ext";
  case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR: return "Fragment shading rate attachment optimal khr";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR: return "Video encode dst khr";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR: return "Video encode src khr";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR: return "Video encode dpb khr";
  case VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT: return "Attachment feedback loop optimal ext";
  case VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM: return "Tensor aliasing arm";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR: return "Video encode quantization map khr";
  case VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT: return "Zero initialized ext";
  case VK_IMAGE_LAYOUT_MAX_ENUM: return "Max enum";
  }
  // clang-format on
}

} // namespace Image