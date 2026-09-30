#pragma once

#include "Graphics/graphicsContext.hpp"
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace Graphics {

struct GraphicsMemory {
  uint64_t timelineValue{};

  std::function<void(const GraphicsContext &, std::shared_ptr<void> &)> destroy;
  std::shared_ptr<void> userdata;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<GraphicsMemory> ReleasedGraphicsMemory;
extern std::mutex ReleasedGraphicsMemoryMutex;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto ProcessReleasedResources(const GraphicsContext &context) -> void;

template <typename T>
auto ScheduleDestruction(
    const std::function<void(const GraphicsContext &, T &)> &destroy,
    uint64_t timelineValue, T userdata) -> void {
  std::lock_guard<std::mutex> lock(ReleasedGraphicsMemoryMutex);

  auto ptr = std::make_shared<T>(std::move(userdata));

  ReleasedGraphicsMemory.emplace_back(GraphicsMemory{
      .timelineValue = timelineValue,
      .destroy = [destroy](const GraphicsContext &context,
                           std::shared_ptr<void> &userdata) -> void {
        T *data = reinterpret_cast<T *>(userdata.get()); // NOLINT

        destroy(context, *data);
      },
      .userdata = ptr,
  });
}

} // namespace Graphics