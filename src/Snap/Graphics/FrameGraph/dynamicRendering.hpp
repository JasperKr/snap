#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
namespace Graphics {

struct GraphicsContext;

auto PrepareRendering(const GraphicsContext &context,
                      VkCommandBuffer vkCommandBuffer,
                      const LoadOpConfig *loadConfig) -> Error;
auto EndRendering(const GraphicsContext &context,
                  VkCommandBuffer vkCommandBuffer) -> void;

} // namespace Graphics