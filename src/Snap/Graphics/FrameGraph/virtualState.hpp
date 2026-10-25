#pragma once

#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Modules/color.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include <vulkan/vulkan_core.h>

namespace Graphics {

struct VirtualState {
  mutable bool dirty = true;

  std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;

  std::vector<VkBuffer> vertexBuffers;
  std::vector<VkDeviceSize> vertexBufferOffsets;

  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
  VkDeviceSize indexBufferOffset{};

  mutable uint64_t hash{};

  void MarkUpdated() { dirty = true; }

  auto GetHash() const -> uint64_t;

  auto operator==(const VirtualState &other) const -> bool { return true; }

  auto ToString() const -> std::string;
};

struct VirtualStateHash {
  auto operator()(const VirtualState &state) const -> uint64_t {
    return state.GetHash();
  };
};

struct VirtualRenderState {
  VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
  VkFrontFace frontFace = VK_FRONT_FACE_MAX_ENUM;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_MAX_ENUM;
  VkBool32 stencilTestEnable = 0;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_MAX_ENUM;
  VkViewport viewport{};
  VkRect2D scissor{};

  mutable bool dirty = true;

  std::string currentDebugMarker;
  std::optional<Color> currentDebugMarkerColor;
  Math::StackVector<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS>
      colorBlendEquations;

  Ref<Shader> shader;
  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
  Math::StackVector<RenderState::RenderTarget, MAX_COLOR_ATTACHMENTS>
      colorAttachments;
  RenderState::RenderTarget depthStencilAttachment;
  bool hasDepthStencilAttachment = false;

  mutable uint64_t hash{};

  void MarkUpdated() { dirty = true; }
  auto GetHash() const -> uint64_t;

  auto operator==(const VirtualRenderState &other) const -> bool {
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

  auto operator==(const VirtualState &other) const -> bool { return true; }

  auto ToString() const -> std::string;
};

struct VirtualRenderStateHash {
  auto operator()(const VirtualRenderState &state) const -> uint64_t {
    return state.GetHash();
  };
};

} // namespace Graphics