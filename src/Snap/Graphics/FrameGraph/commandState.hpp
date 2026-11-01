#pragma once

#include "Graphics/FrameGraph/vulkanResource.hpp"
#include <unordered_set>
#include <vector>
namespace Graphics {

auto GetReadsInternal(
    struct Command &command, struct DrawState *drawState,
    std::vector<struct VulkanResource> &reads,
    std::unordered_set<VulkanResource, VulkanResourceHash<true>> *knownReads)
    -> void;

auto GetWritesInternal(struct Command &command, struct DrawState *drawState,
                       std::vector<struct VulkanResource> &writes) -> void;

auto GetReads(const struct Command &command)
    -> const std::vector<VulkanResource> &;

auto GetWrites(const struct Command &command)
    -> const std::vector<VulkanResource> &;

} // namespace Graphics