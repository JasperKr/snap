#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
namespace Graphics {

auto BindGlobalUBO(const GraphicsContext &context, const Ref<Shader> &shader,
                   VkCommandBuffer commandBuffer) -> Error;

}
