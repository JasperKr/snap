#pragma once

#include <string>
#include <vulkan/vulkan_core.h>

namespace Graphics::VkAccessHelpers {

auto AccessFlags2ToString(VkAccessFlags2 flags) -> std::string;
auto IsAccessFlagReadOnly(VkAccessFlags2 flags) -> bool;
auto IsAccessFlagWriteOnly(VkAccessFlags2 flags) -> bool;
auto IsAccessFlagReadWrite(VkAccessFlags2 flags) -> bool;
auto IsWriteAccess(VkAccessFlags2 flags) -> bool;
auto IsReadAccess(VkAccessFlags2 flags) -> bool;

} // namespace Graphics::VkAccessHelpers