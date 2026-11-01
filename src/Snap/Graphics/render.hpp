#pragma once

#include "Graphics/graphicsState.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/swapchainManager.hpp"
#include "Modules/error.hpp"
#include <string>
#include <vector>
namespace Graphics {

struct StitchInfo {
  // Any amount of buffers to stitch together
  std::array<std::vector<VkCommandBuffer>, FRAMES_IN_FLIGHT> commandBuffers{};
  // std::vector<bool> usedCommandBuffers;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern StitchInfo GlobalStitchInfo;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern SwapchainManager swapchainManager;

auto Present(GraphicsContext &context,
             const std::vector<Ref<Threading::RenderThreadInfo>> &commands)
    -> Error;
auto InitializeRendering(GraphicsContext &context,
                         Window::WindowContext &windowContext) -> Error;
auto DeinitializeRendering(GraphicsContext &context) -> void;
auto UseCommands(uint64_t key) -> void;
auto UseCommands(const std::string &name) -> void;

} // namespace Graphics