#pragma once

#include <string>
#include <tuple>
#include <vulkan/vulkan_core.h>

namespace Graphics::BlendMode {

auto ToString(VkBlendFactor blendFactor) -> std::string_view;
auto ToString(VkBlendOp blendOp) -> std::string_view;
auto ToString(VkBlendFactor srcColorBlendFactor,
              VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp,
              VkBlendFactor srcAlphaBlendFactor,
              VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp)
    -> std::tuple<bool, std::string, std::string>;
auto ToStringProperties(VkPipelineColorBlendAttachmentState state)
    -> std::tuple<bool, std::string, std::string>;
auto ToString(VkPipelineColorBlendAttachmentState state) -> std::string;

} // namespace Graphics::BlendMode