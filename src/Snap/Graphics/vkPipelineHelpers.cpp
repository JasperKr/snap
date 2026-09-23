#include "vkPipelineHelpers.hpp"
#include "Modules/Helpers/utils.hpp"
#include <sstream>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace Graphics::VkPipelineHelpers {

inline auto PipelineStage2ToStringInternal(VkPipelineStageFlags2 pipeline)
    -> std::string_view {
  // clang-format off
  switch (pipeline) {
  case (VK_PIPELINE_STAGE_2_NONE): return "None";
  case (VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT): return "Top Of Pipe";
  case (VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT): return "Draw Indirect";
  case (VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT): return "Vertex Input";
  case (VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT): return "Vertex Shader";
  case (VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT): return "Tessellation Control Shader";
  case (VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT): return "Tessellation Evaluation Shader";
  case (VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT): return "Geometry Shader";
  case (VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT): return "Fragment Shader";
  case (VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT): return "Early Fragment Tests";
  case (VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT): return "Late Fragment Tests";
  case (VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT): return "Color Attachment Output";
  case (VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT): return "Compute Shader";
  case (VK_PIPELINE_STAGE_2_TRANSFER_BIT): return "Transfer";
  case (VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT): return "Bottom Of Pipe";
  case (VK_PIPELINE_STAGE_2_HOST_BIT): return "Host";
  case (VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT): return "All Graphics";
  case (VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT): return "All Commands";
  case (VK_PIPELINE_STAGE_2_COPY_BIT): return "Copy";
  case (VK_PIPELINE_STAGE_2_RESOLVE_BIT): return "Resolve";
  case (VK_PIPELINE_STAGE_2_BLIT_BIT): return "Blit";
  case (VK_PIPELINE_STAGE_2_CLEAR_BIT): return "Clear";
  case (VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT): return "Index Input";
  case (VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT): return "Vertex Attribute Input";
  case (VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT): return "Pre Rasterization Shaders";
  case (VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR): return "Video Decode Khr";
  case (VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR): return "Video Encode Khr";
  default: return "Unknown pipeline";
  }
  // clang-format on
}

auto PipelineStage2ToString(VkPipelineStageFlags2 pipelines) -> std::string {
  std::stringstream stream;

  for (const VkPipelineStageFlags2 pipeline : Utils::BitMaskRange(pipelines)) {
    stream << PipelineStage2ToStringInternal(pipeline) << " | ";
  }

  std::string pipelineStr = stream.str();

  if (!pipelineStr.empty()) {
    pipelineStr.erase(pipelineStr.end() - 3, pipelineStr.end());
  }

  return pipelineStr;
}

} // namespace Graphics::VkPipelineHelpers