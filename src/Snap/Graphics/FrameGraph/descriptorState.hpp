#pragma once

#include "Modules/error.hpp"
#include "Modules/stackVector.hpp"
#include <utility>
#include <vulkan/vulkan_core.h>

namespace Graphics {
auto GetDescriptorSets(const struct GraphicsContext &context)
    -> Result<std::pair<Math::StackVector<VkDescriptorSet, 16>, // NOLINT
                        Math::StackVector<uint32_t, 16>>>;      // NOLINT
}