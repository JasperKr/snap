#include "vkAccessHelpers.hpp"
#include "Modules/Helpers/utils.hpp"
#include <sstream>
#include <string_view>

namespace Graphics {

inline auto AccessFlags2ToStringInternal(VkAccessFlags2 flag)
    -> std::string_view {
  // clang-format off
  switch (flag) {
  case VK_ACCESS_2_NONE: { return "None"; }
  case VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT: { return "Indirect command read"; }
  case VK_ACCESS_2_INDEX_READ_BIT: { return "Index read"; }
  case VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT: { return "Vertex attribute read"; }
  case VK_ACCESS_2_UNIFORM_READ_BIT: { return "Uniform read"; }
  case VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT: { return "Input attachment read"; }
  case VK_ACCESS_2_SHADER_READ_BIT: { return "Shader read"; }
  case VK_ACCESS_2_SHADER_WRITE_BIT: { return "Shader write"; }
  case VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT: { return "Color attachment read"; }
  case VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT: { return "Color attachment write"; }
  case VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT: { return "Depth stencil attachment read"; }
  case VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT: { return "Depth stencil attachment write"; }
  case VK_ACCESS_2_TRANSFER_READ_BIT: { return "Transfer read"; }
  case VK_ACCESS_2_TRANSFER_WRITE_BIT: { return "Transfer write"; }
  case VK_ACCESS_2_HOST_READ_BIT: { return "Host read"; }
  case VK_ACCESS_2_HOST_WRITE_BIT: { return "Host write"; }
  case VK_ACCESS_2_MEMORY_READ_BIT: { return "Memory read"; }
  case VK_ACCESS_2_MEMORY_WRITE_BIT: { return "Memory write"; }
  case VK_ACCESS_2_SHADER_SAMPLED_READ_BIT: { return "Shader sampled read"; }
  case VK_ACCESS_2_SHADER_STORAGE_READ_BIT: { return "Shader storage read"; }
  case VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT: { return "Shader storage write"; }
  case VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR: { return "Video decode read khr"; }
  case VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR: { return "Video decode write khr"; }
  case VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR: { return "Video encode read khr"; }
  case VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR: { return "Video encode write khr"; }
  case VK_ACCESS_2_SHADER_TILE_ATTACHMENT_READ_BIT_QCOM: { return "Shader tile attachment read qcom"; }
  case VK_ACCESS_2_SHADER_TILE_ATTACHMENT_WRITE_BIT_QCOM: { return "Shader tile attachment write qcom"; }
  case VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT: { return "Transform feedback write ext"; }
  case VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT: { return "Transform feedback counter read ext"; }
  case VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT: { return "Transform feedback counter write ext"; }
  case VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT: { return "Conditional rendering read ext"; }
  case VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_EXT: { return "Command preprocess read ext"; }
  case VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_EXT: { return "Command preprocess write ext"; }
  case VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR: { return "Fragment shading rate attachment read khr"; }
  case VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR: { return "Acceleration structure read khr"; }
  case VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR: { return "Acceleration structure write khr"; }
  case VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT: { return "Fragment density map read ext"; }
  case VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT: { return "Color attachment read noncoherent ext"; }
  case VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT: { return "Descriptor buffer read ext"; }
  case VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI: { return "Invocation mask read huawei"; }
  case VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR: { return "Shader binding table read khr"; }
  case VK_ACCESS_2_MICROMAP_READ_BIT_EXT: { return "Micromap read ext"; }
  case VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT: { return "Micromap write ext"; }
  case VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV: { return "Optical flow read nv"; }
  case VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV: { return "Optical flow write nv"; }
  case VK_ACCESS_2_DATA_GRAPH_READ_BIT_ARM: { return "Data graph read arm"; }
  case VK_ACCESS_2_DATA_GRAPH_WRITE_BIT_ARM: { return "Data graph write arm"; }
  default:
  return "Unknown Access Flag";
    // clang-format on
  }
}

auto AccessFlags2ToString(VkAccessFlags2 flags) -> std::string {
  std::stringstream stream;

  for (const VkAccessFlags2 access : Utils::BitMaskRange(flags)) {
    stream << AccessFlags2ToStringInternal(access) << " | ";
  }

  std::string accessStr = stream.str();

  if (!accessStr.empty()) {
    accessStr.erase(accessStr.end() - 3, accessStr.end());
  }

  return accessStr;
}

static constexpr VkAccessFlags2 writeAccesses =
    VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_HOST_WRITE_BIT |
    VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
    VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR |
    VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR |
    VK_ACCESS_2_SHADER_TILE_ATTACHMENT_WRITE_BIT_QCOM |
    VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT |
    VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT |
    VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_EXT |
    VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
    VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT | VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV |
    VK_ACCESS_2_DATA_GRAPH_WRITE_BIT_ARM;

static constexpr VkAccessFlags2 readAccesses =
    VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT |
    VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT |
    VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT |
    VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
    VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_HOST_READ_BIT |
    VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT |
    VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
    VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR |
    VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR |
    VK_ACCESS_2_SHADER_TILE_ATTACHMENT_READ_BIT_QCOM |
    VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT |
    VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT |
    VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_EXT |
    VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR |
    VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
    VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT |
    VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT |
    VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI |
    VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR |
    VK_ACCESS_2_MICROMAP_READ_BIT_EXT | VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV |
    VK_ACCESS_2_DATA_GRAPH_READ_BIT_ARM;

auto IsAccessFlagReadOnly(VkAccessFlags2 flags) -> bool {
  return (flags | writeAccesses) == 0U && (flags | readAccesses) != 0U;
}

auto IsAccessFlagWriteOnly(VkAccessFlags2 flags) -> bool {
  return (flags | readAccesses) == 0U && (flags | writeAccesses) != 0U;
}

// Effectively flags != 0U
auto IsAccessFlagReadWrite(VkAccessFlags2 flags) -> bool {
  return (flags | readAccesses) != 0U && (flags | writeAccesses) != 0U;
}

auto IsWriteAccess(VkAccessFlags2 flags) -> bool {
  return (flags | writeAccesses) != 0U;
}

auto IsReadAccess(VkAccessFlags2 flags) -> bool {
  return (flags | readAccesses) != 0U;
}

} // namespace Graphics