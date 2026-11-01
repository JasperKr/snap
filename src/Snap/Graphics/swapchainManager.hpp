#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "Modules/window.hpp"

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <vector>
namespace Graphics {

struct OldSwapchain {
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  Math::StackVector<Ref<Texture>, MaxImageCount> textures;
  uint64_t lastFrameUsed = 0;

  Math::StackVector<VkSemaphore, MaxImageCount> imageReady;
};

class SwapchainManager {
public:
  static auto Initialize(GraphicsContext &context,
                         Window::WindowContext &windowContext)
      -> Result<SwapchainManager>;
  auto Deinitialize(GraphicsContext &context) -> void;
  auto RecreateSwapchain(GraphicsContext &context,
                         Window::WindowContext &wcontext) -> Error;
  auto CleanupOldSwapchains(GraphicsContext &context, uint64_t currentFrame)
      -> void;
  auto GetCurrentSwapchain() -> VkSwapchainKHR;
  auto GetCurrentSwapchainTexture(const GraphicsContext &context)
      -> Ref<Texture>;

  auto NewFrame(GraphicsContext &context, Window::WindowContext &windowContext,
                uint64_t currentFrame) -> Error;
  auto EndFrame(const GraphicsContext &context) -> Error;

  auto MakeDirty() -> void;

private:
  auto CreateVkSwapchain(GraphicsContext &context,
                         Window::WindowContext &windowContext) -> Error;

  // To be used when recreating swapchains to avoid destroying in-use swapchains
  std::vector<OldSwapchain> oldSwapchains;

  VkSwapchainKHR currentSwapchain = VK_NULL_HANDLE;
  Math::StackVector<Ref<Texture>, MaxImageCount> currentTextures;
  uint64_t lastFrameUsed = 0;

  bool isDirty = false;
};

} // namespace Graphics