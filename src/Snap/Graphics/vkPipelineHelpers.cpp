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
  case VK_PIPELINE_STAGE_2_NONE: return "none";
  case VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT: return "top_of_pipe_bit";
  case VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT: return "draw_indirect_bit";
  case VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT: return "vertex_input_bit";
  case VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT: return "vertex_shader_bit";
  case VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT: return "tessellation_control_shader_bit";
  case VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT: return "tessellation_evaluation_shader_bit";
  case VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT: return "geometry_shader_bit";
  case VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT: return "fragment_shader_bit";
  case VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT: return "early_fragment_tests_bit";
  case VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT: return "late_fragment_tests_bit";
  case VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT: return "color_attachment_output_bit";
  case VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT: return "compute_shader_bit";
  case VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT: return "all_transfer_bit";
  case VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT: return "bottom_of_pipe_bit";
  case VK_PIPELINE_STAGE_2_HOST_BIT: return "host_bit";
  case VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT: return "all_graphics_bit";
  case VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT: return "all_commands_bit";
  case VK_PIPELINE_STAGE_2_COPY_BIT: return "copy_bit";
  case VK_PIPELINE_STAGE_2_RESOLVE_BIT: return "resolve_bit";
  case VK_PIPELINE_STAGE_2_BLIT_BIT: return "blit_bit";
  case VK_PIPELINE_STAGE_2_CLEAR_BIT: return "clear_bit";
  case VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT: return "index_input_bit";
  case VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT: return "vertex_attribute_input_bit";
  case VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT: return "pre_rasterization_shaders_bit";
  case VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR: return "video_decode_bit_khr";
  case VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR: return "video_encode_bit_khr";
  case VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT: return "transform_feedback_bit_ext";
  case VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT: return "conditional_rendering_bit_ext";
  case VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV: return "command_preprocess_bit_nv";
  case VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR: return "fragment_shading_rate_attachment_bit_khr";
  case VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR: return "acceleration_structure_build_bit_khr";
  case VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR: return "ray_tracing_shader_bit_khr";
  case VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT: return "fragment_density_process_bit_ext";
  case VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV: return "task_shader_bit_nv";
  case VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV: return "mesh_shader_bit_nv";
  case VK_PIPELINE_STAGE_2_SUBPASS_SHADER_BIT_HUAWEI: return "subpass_shader_bit_huawei";
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