#pragma once

#include "Graphics/deviceSettings.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Graphics/threadContext.hpp"
#include "Modules/window.hpp"

#include "Modules/error.hpp"
#include <mutex>
#include <string_view>
#include <vector>

#include "volk/volk.h"

#include "vulkan/vulkan_core.h"
#include <vulkan/vulkan.h>

namespace Graphics {

auto Initialize(GraphicsContext &context, Window::WindowContext &wcontext,
                const DeviceSettings &deviceSettings) -> Error;
auto GetThreadContext() -> ThreadContext &;
auto GetVirtualCommandBuffer() -> VirtualCommandBuffer *;
auto GetVkCommandBuffer() -> VkCommandBuffer;
void Deinitialize(GraphicsContext &context);

auto BeginSingleTimeCommands(const GraphicsContext &context) -> VkCommandBuffer;

auto EndSingleTimeCommands(const GraphicsContext &context,
                           VkCommandBuffer commandBuffer) -> void;

// Graphics context NOLINTNEXTLINE
extern GraphicsContext *g_ctx;
void SetCurrentGraphicsContext(GraphicsContext *ctx);
auto GetCurrentGraphicsContext() -> GraphicsContext *;

auto GetDeferredDestructionAllowed() -> bool &;

auto PushDebugMarker(const std::string_view &name,
                     const struct Color *color = nullptr) -> void;
auto PopDebugMarker() -> void;
auto PushDebugLabel(const std::string_view &name,
                    const struct Color *color = nullptr) -> void;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern SemaphoreManager semaphoreManager;
thread_local extern std::string ContextDebugname;

extern std::vector<VkCommandPool> CommandPools;
extern std::mutex CommandPoolsMutex;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

} // namespace Graphics