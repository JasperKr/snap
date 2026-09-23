#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/sampler.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <sys/types.h>

#include <vulkan/vulkan_core.h>

namespace Graphics {
struct VirtualCommandBuffer;
}

namespace Graphics::Threading {

struct RenderThreadData {
  uint64_t key = 0;
  int64_t priority = 0; // Tie-breaker for overlapping keys

  std::shared_ptr<::Graphics::VirtualCommandBuffer> commandBuffer;
  uint64_t cmdBufferTimelineValue{};
  uint64_t acquiredAtFrame{};
  uint32_t queueFamily{};

  std::string name;
  uint64_t id{};
};

const Type renderInfoType = Type("Commands");

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

struct RenderThreadInfo : Object {
  RenderThreadInfo(const RenderThreadInfo &) = delete;
  RenderThreadInfo(RenderThreadInfo &&) = delete;
  auto operator=(const RenderThreadInfo &) -> RenderThreadInfo & = delete;
  auto operator=(RenderThreadInfo &&) -> RenderThreadInfo & = delete;
  explicit RenderThreadInfo() : threadData() {}
  ~RenderThreadInfo() override = default;

  RenderThreadData threadData;

  static auto GetType() -> Type const * { return &renderInfoType; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return &renderInfoType;
  }
};

extern thread_local Ref<RenderThreadInfo> CurrentRenderThreadInfo;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

struct AcquireInfo {
  std::string name;
  int64_t priority;

  uint32_t queueFamily;
};

// Acquire a command buffer for the current thread, must have rendering permission
auto AcquireCommandBuffer(Graphics::GraphicsContext &context,
                          const AcquireInfo &info)
    -> Result<Ref<RenderThreadInfo>>;

// Submit the commands recorded on the current thread
auto SubmitCommands(Graphics::GraphicsContext &context)
    -> Result<Ref<RenderThreadInfo>>;

// Initialize the render threading module
auto Initialize(Graphics::GraphicsContext &context) -> Error;

// Deinitialize the render threading module
auto Deinitialize(Graphics::GraphicsContext &context) -> Error;

struct GraphicsConfiguration {
  SamplerDescription defaultSamplerDescription{};
};

auto GetGraphicsConfiguration() -> GraphicsConfiguration &;

} // namespace Graphics::Threading