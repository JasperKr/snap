#pragma once

#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace Graphics::RenderState {

inline auto CompareVkPipelineColorBlendAttachmentState(
    const VkPipelineColorBlendAttachmentState &first,
    const VkPipelineColorBlendAttachmentState &second) -> bool {
  return first.blendEnable == second.blendEnable &&
         first.srcColorBlendFactor == second.srcColorBlendFactor &&
         first.dstColorBlendFactor == second.dstColorBlendFactor &&
         first.colorBlendOp == second.colorBlendOp &&
         first.srcAlphaBlendFactor == second.srcAlphaBlendFactor &&
         first.dstAlphaBlendFactor == second.dstAlphaBlendFactor &&
         first.alphaBlendOp == second.alphaBlendOp &&
         first.colorWriteMask == second.colorWriteMask;
}

const static Type LuaRendertargetType = Type("RenderTarget");

struct RenderTarget {
  VkPipelineColorBlendAttachmentState blendMode = BlendmodeNone;
  VkClearValue clearValue = {};
  Ref<Texture> texture;
  int location = -1; // Default to index in the render target array
  mutable bool dirty = true;

  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  mutable uint64_t hash;
  auto GetHash() const -> uint64_t;

  auto operator==(const RenderTarget &other) const -> bool {
    if (texture->getID() != other.texture->getID()) {
      return false;
    }

    if (loadOp != other.loadOp) {
      return false;
    }

    if (!CompareVkPipelineColorBlendAttachmentState(blendMode,
                                                    other.blendMode)) {
      return false;
    }

    return location == other.location;
  }
};

struct LuaRenderTarget : Object {
  explicit LuaRenderTarget(RenderTarget renderTarget)
      : renderTarget(std::move(renderTarget)) {}

  RenderTarget renderTarget;

  static auto GetType() -> Type const * { return &LuaRendertargetType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return LuaRenderTarget::GetType();
  }
};

} // namespace Graphics::RenderState