#include "resource.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Modules/Helpers/utils.hpp"
#include <cassert>
#include <cstdint>
#include <mutex>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

std::vector<GraphicsMemory> ReleasedGraphicsMemory{};

std::mutex ReleasedGraphicsMemoryMutex{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

inline auto CanBeDestroyed( // NOLINTNEXTLINE
    const uint64_t &resourceTimelineValue) -> bool {

  return !Graphics::semaphoreManager.IsInUse(resourceTimelineValue);
}

auto ProcessReleasedResources(const GraphicsContext &context) -> void {
  if (!GetDeferredDestructionAllowed()) {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(
        ReleasedGraphicsMemoryMutex, Graphics::GraphicsContext::mutexes.device,
        Graphics::GraphicsContext::mutexes.vmaAllocator);

    for (auto &res : ReleasedGraphicsMemory) {
      res.destroy(context, res.userdata);
    }

    ReleasedGraphicsMemory.clear();

    return;
  }

  {
    std::scoped_lock<std::mutex, std::mutex, std::mutex> lock(
        Graphics::GraphicsContext::mutexes.device,
        Graphics::GraphicsContext::mutexes.vmaAllocator,
        ReleasedGraphicsMemoryMutex);

    Utils::UnorderedErase(ReleasedGraphicsMemory,
                          [context](GraphicsMemory &res) -> auto {
                            if (CanBeDestroyed(res.timelineValue)) {
                              res.destroy(context, res.userdata);
                              return true;
                            }
                            return false;
                          });
  }
}

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

} // namespace Graphics