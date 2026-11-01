#pragma once

#include "Modules/error.hpp"
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto Flush(const struct GraphicsContext &context,
           VkCommandBuffer vkCommandBuffer) -> Result<bool>;
auto GetTargetSize() -> VkExtent2D;
auto BeginFrame(const struct GraphicsContext &context) -> Error;

} // namespace Graphics