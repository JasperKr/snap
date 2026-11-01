#include "virtualState.hpp"
#include "Graphics/blendmode.hpp"
#include "Graphics/renderState.hpp"
#include "Modules/Helpers/hasher.hpp"
#include <cstdint>
#include <format>
#include <sstream>
#include <string>
#include <string_view>

namespace Graphics {

inline auto ToString(VkCullModeFlags cullMode) -> std::string_view {
  switch ((VkCullModeFlagBits)cullMode) {
  case VK_CULL_MODE_NONE:
    return "VK_CULL_MODE_NONE";
  case VK_CULL_MODE_FRONT_BIT:
    return "VK_CULL_MODE_FRONT_BIT";
  case VK_CULL_MODE_BACK_BIT:
    return "VK_CULL_MODE_BACK_BIT";
  case VK_CULL_MODE_FRONT_AND_BACK:
    return "VK_CULL_MODE_FRONT_AND_BACK";
  case VK_CULL_MODE_FLAG_BITS_MAX_ENUM:
    return "VK_CULL_MODE_FLAG_BITS_MAX_ENUM";
  }
}

inline auto ToString(VkFrontFace face) -> std::string_view {
  switch (face) {
  case VK_FRONT_FACE_COUNTER_CLOCKWISE:
    return "VK_FRONT_FACE_COUNTER_CLOCKWISE";
  case VK_FRONT_FACE_CLOCKWISE:
    return "VK_FRONT_FACE_CLOCKWISE";
  case VK_FRONT_FACE_MAX_ENUM:
    return "VK_FRONT_FACE_MAX_ENUM";
  }
}

inline auto ToString(VkCompareOp compareOp) -> std::string_view {
  switch (compareOp) {
  case VK_COMPARE_OP_NEVER:
    return "VK_COMPARE_OP_NEVER";
  case VK_COMPARE_OP_LESS:
    return "VK_COMPARE_OP_LESS";
  case VK_COMPARE_OP_EQUAL:
    return "VK_COMPARE_OP_EQUAL";
  case VK_COMPARE_OP_LESS_OR_EQUAL:
    return "VK_COMPARE_OP_LESS_OR_EQUAL";
  case VK_COMPARE_OP_GREATER:
    return "VK_COMPARE_OP_GREATER";
  case VK_COMPARE_OP_NOT_EQUAL:
    return "VK_COMPARE_OP_NOT_EQUAL";
  case VK_COMPARE_OP_GREATER_OR_EQUAL:
    return "VK_COMPARE_OP_GREATER_OR_EQUAL";
  case VK_COMPARE_OP_ALWAYS:
    return "VK_COMPARE_OP_ALWAYS";
  case VK_COMPARE_OP_MAX_ENUM:
    return "VK_COMPARE_OP_MAX_ENUM";
  }
}

inline auto ToString(VkPolygonMode mode) -> std::string_view {
  switch (mode) {
  case VK_POLYGON_MODE_FILL:
    return "VK_POLYGON_MODE_FILL";
  case VK_POLYGON_MODE_LINE:
    return "VK_POLYGON_MODE_LINE";
  case VK_POLYGON_MODE_POINT:
    return "VK_POLYGON_MODE_POINT";
  case VK_POLYGON_MODE_FILL_RECTANGLE_NV:
    return "VK_POLYGON_MODE_FILL_RECTANGLE_NV";
  case VK_POLYGON_MODE_MAX_ENUM:
    return "VK_POLYGON_MODE_MAX_ENUM";
  }
}

inline auto ToString(VkPrimitiveTopology topology) -> std::string_view {
  switch (topology) {
  case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    return "VK_PRIMITIVE_TOPOLOGY_POINT_LIST";
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    return "VK_PRIMITIVE_TOPOLOGY_LINE_LIST";
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    return "VK_PRIMITIVE_TOPOLOGY_LINE_STRIP";
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST";
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP";
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN";
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    return "VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY";
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    return "VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY";
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY";
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    return "VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY";
  case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    return "VK_PRIMITIVE_TOPOLOGY_PATCH_LIST";
  case VK_PRIMITIVE_TOPOLOGY_MAX_ENUM:
    return "VK_PRIMITIVE_TOPOLOGY_MAX_ENUM";
  }
}

inline auto ToString(VkPipelineBindPoint bindPoint) -> std::string_view {
  switch (bindPoint) {
  case VK_PIPELINE_BIND_POINT_GRAPHICS:
    return "VK_PIPELINE_BIND_POINT_GRAPHICS";
  case VK_PIPELINE_BIND_POINT_COMPUTE:
    return "VK_PIPELINE_BIND_POINT_COMPUTE";
  case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
    return "VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR";
  case VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI:
    return "VK_PIPELINE_BIND_POINT_SUBPASS_SHADING_HUAWEI";
  case VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM:
    return "VK_PIPELINE_BIND_POINT_DATA_GRAPH_ARM";
  case VK_PIPELINE_BIND_POINT_MAX_ENUM:
    return "VK_PIPELINE_BIND_POINT_MAX_ENUM";
  }
}

inline auto ToString(VkAttachmentLoadOp loadOp) -> std::string_view {
  switch (loadOp) {
  case VK_ATTACHMENT_LOAD_OP_LOAD:
    return "VK_ATTACHMENT_LOAD_OP_LOAD";
  case VK_ATTACHMENT_LOAD_OP_CLEAR:
    return "VK_ATTACHMENT_LOAD_OP_CLEAR";
  case VK_ATTACHMENT_LOAD_OP_DONT_CARE:
    return "VK_ATTACHMENT_LOAD_OP_DONT_CARE";
  case VK_ATTACHMENT_LOAD_OP_NONE:
    return "VK_ATTACHMENT_LOAD_OP_NONE";
  case VK_ATTACHMENT_LOAD_OP_MAX_ENUM:
    return "VK_ATTACHMENT_LOAD_OP_MAX_ENUM";
  }
}

inline auto ToString(VkViewport viewport) -> std::string {
  return std::format("size: {}x{}, offset: ({}, {})", viewport.width,
                     viewport.height, viewport.x, viewport.y);
}

inline auto ToString(VkRect2D rect) -> std::string {
  return std::format("size: {}x{}, offset: ({}, {})", rect.extent.width,
                     rect.extent.height, rect.offset.x, rect.offset.y);
}

inline auto ToString(VkClearValue clearValue, bool isDepthStencil)
    -> std::string {
  if (!isDepthStencil) {
    return std::format("[{}, {}, {}, {}]", clearValue.color.float32[0],
                       clearValue.color.float32[1], clearValue.color.float32[2],
                       clearValue.color.float32[3]);
  }

  return std::format("depth: {}, stencil: {}", clearValue.depthStencil.depth,
                     clearValue.depthStencil.stencil);
}

auto VirtualRenderState::ToString() const -> std::string {
  std::stringstream stream;
  // stream << "Cull mode: " << ::Graphics::ToString(cullMode) << "\n";
  // stream << "Front face: " << ::Graphics::ToString(frontFace) << "\n";
  // stream << "Depth test enable: " << depthTestEnable << "\n";
  // stream << "Depth write enable: " << depthWriteEnable << "\n";
  stream << "Stencil test enable: " << stencilTestEnable << "\n";
  // stream << "Depth compare: " << ::Graphics::ToString(depthCompareOp) << "\n";
  stream << "Polygon mode: " << ::Graphics::ToString(polygonMode) << "\n";
  // stream << "Viewport: " << ::Graphics::ToString(viewport) << "\n";
  // stream << "Scissor: " << ::Graphics::ToString(scissor) << "\n";
  stream << "Shader: " << shader->name << "\n";
  stream << "Primitive topology: " << ::Graphics::ToString(primitiveTopology)
         << "\n";
  stream << "Bind point: " << ::Graphics::ToString(bindPoint) << "\n";
  stream << std::format("Color attachments (#{}): \n", colorAttachments.size());

  for (const auto &attachment : colorAttachments) {
    stream << "{\n\tTexture: " << attachment.texture->debugName << "\n";
    stream << "\tLocation: " << attachment.location << "\n";
    stream << "\tLoad Op: " << ::Graphics::ToString(attachment.loadOp) << "\n";
    stream << "\t" << BlendMode::ToString(attachment.blendMode) << "\n";
    stream << "\tClear color: "
           << ::Graphics::ToString(attachment.clearValue, false) << "\n";
    stream << "}" << (attachment != colorAttachments.back() ? ",\n" : "\n");
  }

  if (!hasDepthStencilAttachment) {
    stream << "No depth / stencil attachment.";
  } else {
    const auto &attachment = depthStencilAttachment;
    stream << "Depth / stencil attachment: \n";
    stream << "{\n\tTexture: " << attachment.texture->debugName << "\n";
    stream << "\tLocation: " << attachment.location << "\n";
    stream << "\tLoad Op: " << ::Graphics::ToString(attachment.loadOp) << "\n";
    stream << "\t" << BlendMode::ToString(attachment.blendMode) << "\n";
    stream << "\tClear: " << ::Graphics::ToString(attachment.clearValue, false)
           << "\n";
    stream << "}\n";
  }

  return stream.str();
}

auto VirtualRenderState::operator==(const RenderState::State &other) const
    -> bool {
  if (colorAttachments != other.colorAttachments) {
    return false;
  }

  if (shader != other.shader) {
    return false;
  }

  if (stencilTestEnable != other.stencilTestEnable ||
      polygonMode != other.polygonMode ||
      primitiveTopology != other.primitiveTopology ||
      bindPoint != other.bindPoint) {
    return false;
  }

  if (hasDepthStencilAttachment != other.hasDepthStencilAttachment) {
    return false;
  }

  if (hasDepthStencilAttachment) {
    if (depthStencilAttachment != other.depthStencilAttachment) {
      return false;
    }
  }

  return true;
}

auto VirtualRenderState::GetHash() const -> uint64_t {
  Hash::Hasher hash{};

  for (const auto &attachment : colorAttachments) {
    hash.Add(attachment.GetHash());
  }

  hash.Add(shader->getID());
  hash.Add(stencilTestEnable);
  hash.Add((uint64_t)polygonMode);
  hash.Add((uint64_t)primitiveTopology);
  hash.Add((uint64_t)bindPoint);
  hash.Add(hasDepthStencilAttachment ? depthStencilAttachment.GetHash() : 0UL);

  return hash.Get();
}

} // namespace Graphics