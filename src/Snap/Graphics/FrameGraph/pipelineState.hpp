#pragma once

#include "Modules/error.hpp"
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto Flush(const struct GraphicsContext &context,
           VkCommandBuffer vkCommandBuffer) -> Result<bool>;
auto GetTargetSize() -> VkExtent2D;
auto GetMaximumAllowedViewport() -> VkViewport;
auto GetScissor() -> VkRect2D;
auto GetClippedViewport() -> VkViewport;
auto GetViewport() -> VkViewport;
auto BeginFrame(const struct GraphicsContext &context) -> Error;

} // namespace Graphics