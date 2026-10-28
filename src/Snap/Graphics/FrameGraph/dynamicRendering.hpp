#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
namespace Graphics {

struct GraphicsContext;

auto PrepareRendering(const GraphicsContext &context,
                      VkCommandBuffer vkCommandBuffer) -> Error;
auto EndRendering(const GraphicsContext &context,
                  VkCommandBuffer vkCommandBuffer) -> void;

} // namespace Graphics