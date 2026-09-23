#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

namespace Graphics {

struct RendertargetKey {
  ObjectID textureID;
  int location;
  int layer;

  auto operator==(const RendertargetKey &other) const -> bool {
    return textureID == other.textureID && location == other.location &&
           layer == other.layer;
  }
};

struct PipelineLayout {
  VkPipelineLayout layout;
  Math::StackVector<VkDescriptorSetLayout, 16> descriptorSetLayouts; // NOLINT
};

struct StateKey {
  Math::StackVector<RendertargetKey, MAX_COLOR_ATTACHMENTS> colorAttachments;
  bool hasDepthStencilAttachment;
  RendertargetKey depthStencilTextureID{};
  ObjectID shaderModuleID;
  VkPipelineBindPoint bindPoint;
  VkBool32 stencilTestEnable;
  VkPolygonMode polygonMode;
  VkPrimitiveTopology primitiveTopology;

  explicit StateKey(const RenderState::State &state)
      : hasDepthStencilAttachment(state.hasDepthStencilAttachment),
        shaderModuleID(state.shader->getID()), bindPoint(state.bindPoint),
        stencilTestEnable(state.stencilTestEnable),
        polygonMode(state.polygonMode),
        primitiveTopology(state.primitiveTopology) {

    colorAttachments.fill({.textureID = 0, .location = -1, .layer = -1});
    depthStencilTextureID = {.textureID = 0, .location = -1};

    for (int i = 0; i < state.colorAttachments.size(); i++) {
      const auto &attachment = state.colorAttachments.at(i);
      colorAttachments.at(i) = RendertargetKey{
          .textureID = attachment.texture->getID(),
          .location = attachment.location,
      };
    }

    if (state.hasDepthStencilAttachment) {
      depthStencilTextureID = RendertargetKey{
          .textureID = state.depthStencilAttachment.texture->getID(),
          .location = 0,
      };
    }
  }

  auto operator==(const StateKey &other) const -> bool {
    if (hasDepthStencilAttachment != other.hasDepthStencilAttachment ||
        shaderModuleID != other.shaderModuleID ||
        bindPoint != other.bindPoint ||
        stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology ||
        colorAttachments != other.colorAttachments) {
      return false;
    }

    if (hasDepthStencilAttachment &&
        !(depthStencilTextureID == other.depthStencilTextureID)) {
      return false;
    }

    return true;
  }
};

struct StateKeyHash {
  static auto Hash(const StateKey &state) -> size_t {
    Hash::Hasher hasher{};

    // Special case for compute pipelines
    if (state.bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
      hasher.Add(std::hash<VkPipelineBindPoint>()(state.bindPoint));
      hasher.Add(state.shaderModuleID);
      return hasher.Get();
    }

    if (state.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
      PrintError("Trying to hash unsupported pipeline bind point.");
    }

    hasher.Add(std::hash<bool>()(state.stencilTestEnable == 1));
    hasher.Add(std::hash<VkPolygonMode>()(state.polygonMode));
    hasher.Add(std::hash<VkPipelineBindPoint>()(state.bindPoint));
    hasher.Add(std::hash<VkPrimitiveTopology>()(state.primitiveTopology));
    hasher.Add(state.shaderModuleID);

    for (const auto &attachment : state.colorAttachments) {
      hasher.Add(attachment.textureID);
      hasher.Add(attachment.location);
      hasher.Add(attachment.layer);
    }

    return hasher.Get();
  }

  auto operator()(const StateKey &state) const -> size_t { return Hash(state); }
};

struct PipelineCache {
  std::mutex mutex;
  LRUCache<StateKey, std::pair<VkPipeline, PipelineLayout>, StateKeyHash> cache{
      1};
  std::unordered_map<ObjectID, PipelineLayout> layouts;

  PipelineLayout currentLayout;
  std::vector<VkPipeline> pipelines;
  std::vector<PipelineLayout> pipelineLayouts;

  auto Initialize(const GraphicsContext &context) -> Error;
  auto DeInitialize(const GraphicsContext &context) -> void;

  auto GetPipelineLayout(const GraphicsContext &context, Shader *shader)
      -> Result<PipelineLayout>;
};

auto GetPipelineCache() -> PipelineCache &;

} // namespace Graphics