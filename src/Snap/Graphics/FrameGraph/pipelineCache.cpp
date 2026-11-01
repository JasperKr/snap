#include "pipelineCache.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Libraries/vma.hpp"
#include "Modules/error.hpp"

namespace Graphics {

constexpr size_t PipelineCacheSize = 512UL;

auto PipelineCache::Initialize(const GraphicsContext &context) -> Error {
  cache = std::move(LRUCache<StateKey, VkPipeline, StateKeyHash>(
      PipelineCacheSize, [](const StateKey &key, VkPipeline &value) -> void {
        ScheduleDestruction<VkPipeline>(
            [](const GraphicsContext &context, VkPipeline &pipeline) -> auto {
              vkDestroyPipeline(context.device, pipeline,
                                GetAllocationCallbacks());
            },
            Graphics::SemaphoreManager::GetSemaphoreValue(), value);
      }));

  return {};
}

auto PipelineCache::DeInitialize(const GraphicsContext &context) -> void {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device, mutex);
  for (const auto &pipeline : pipelines) {
    if (pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(context.device, pipeline, GetAllocationCallbacks());
    }
  }

  cache.clear();
  pipelines.clear();
}

auto GetPipelineCache() -> PipelineCache & {
  static PipelineCache cache{};

  return cache;
}

} // namespace Graphics