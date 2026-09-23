#include "pipelineCache.hpp"
#include "Graphics/FrameGraph/descriptorCache.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include <utility>

namespace Graphics {

constexpr size_t PipelineCacheSize = 512UL;

auto PipelineCache::Initialize(const GraphicsContext &context) -> Error {
  cache = std::move(
      LRUCache<StateKey, std::pair<VkPipeline, PipelineLayout>, StateKeyHash>(
          PipelineCacheSize,
          [](const StateKey &key,
             std::pair<VkPipeline, PipelineLayout> &value) -> void {
            PipelineMemory pipelineMemory{
                .pipeline = value.first,
            };
            ScheduleDestruction(
                pipelineMemory,
                Graphics::SemaphoreManager::GetSemaphoreValue());
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

  for (const auto &layout : pipelineLayouts) {
    vkDestroyPipelineLayout(context.device, layout.layout,
                            GetAllocationCallbacks());
  }

  pipelines.clear();
  pipelineLayouts.clear();
}

auto TryCreateShaderDescriptorBindingInfo(const GraphicsContext &context,
                                          Shader *shader) -> Error {
  ZoneScoped;

  // No need to fill if already done
  if (!shader->bindingInfos.empty()) {
    return Error::Success();
  }

  for (auto &layout : shader->reflection.resources) {
    if (layout.IsBuffer()) {
      auto &bufferInfo = std::get<Reflect::BufferInfo>(layout.info);

      if (bufferInfo.bufferType == Reflect::BufferType::Uniform ||
          bufferInfo.bufferType == Reflect::BufferType::Storage) {
        auto layoutBinding = VkDescriptorSetLayoutBinding{
            .binding = bufferInfo.binding,
            .descriptorType =
                bufferInfo.bufferType == Reflect::BufferType::Uniform
                    ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                    : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
            .pImmutableSamplers = nullptr,
        };

        shader->bindingInfos[bufferInfo.set].emplace_back(layoutBinding);
      }
    } else if (layout.IsSampler()) {
      auto &imageInfo = std::get<Reflect::SamplerInfo>(layout.info);

      auto layoutBinding = VkDescriptorSetLayoutBinding{
          .binding = imageInfo.binding,
          .descriptorType = imageInfo.access == SLANG_RESOURCE_ACCESS_READ
                                ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_ALL,
          .pImmutableSamplers = nullptr,
      };

      shader->bindingInfos[imageInfo.set].emplace_back(layoutBinding);
    } else if (layout.IsAccelerationStructure()) {
      auto &accelStructInfo =
          std::get<Reflect::AccelerationStructureInfo>(layout.info);

      auto layoutBinding = VkDescriptorSetLayoutBinding{
          .binding = accelStructInfo.binding,
          .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_ALL,
          .pImmutableSamplers = nullptr,
      };

      shader->bindingInfos[accelStructInfo.set].emplace_back(layoutBinding);
    }
  }

  if (shader->reflection.hasGlobals) {
    auto layoutBinding = VkDescriptorSetLayoutBinding{
        .binding = shader->reflection.globals.binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
        .pImmutableSamplers = nullptr,
    };

    shader->bindingInfos[shader->reflection.globals.set].emplace_back(
        layoutBinding);
  }

  for (auto &setPair : shader->bindingInfos) {
    std::ranges::sort(setPair.second,
                      [](const VkDescriptorSetLayoutBinding &first,
                         const VkDescriptorSetLayoutBinding &second) -> bool {
                        return first.binding < second.binding;
                      });
  }

  return Error::Success();
}

auto PipelineCache::GetPipelineLayout(const GraphicsContext &context,
                                      Shader *shader)
    -> Result<PipelineLayout> {
  ZoneScoped;

  auto layoutIter = layouts.find(shader->getID());

  if (layoutIter != layouts.end()) {
    return layoutIter->second;
  }

  auto pushConstantRanges = std::vector<VkPushConstantRange>{};

  CHECK_NULL(shader);

  if (shader->pushBuffer) {
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = shader->pushBuffer->GetStageFlags();
    pushConstantRange.offset = shader->pushBuffer->GetBufferOffset();
    pushConstantRange.size =
        static_cast<uint32_t>(shader->pushBuffer->GetBufferSize());
    if (pushConstantRange.size == 0) {
      return Error::Unexpected(
          "Push buffer has zero size in shader reflection");
    }

    PrintDebug("Adding push constant range: offset {}, size {}",
               pushConstantRange.offset, pushConstantRange.size);

    pushConstantRanges.emplace_back(pushConstantRange);
  }

  PrintDebug("Creating pipeline layout from shader reflection");

  Math::StackVector<VkDescriptorSetLayout, 16> setLayouts{}; // NOLINT

  CHECK_ERR(TryCreateShaderDescriptorBindingInfo(context, shader));

  std::unordered_set<uint32_t> uniqueSets{};
  bool hasBindings = false;
  for (const auto &setPair : shader->bindingInfos) {
    uniqueSets.insert(setPair.first);
    hasBindings = true;
  }

  uint32_t setCount = uniqueSets.size();
  uint32_t maxSet = 0;
  auto maxSetIter = std::ranges::max_element(uniqueSets);
  if (maxSetIter != uniqueSets.end()) {
    maxSet = std::max(setCount - 1, *maxSetIter);
  }

  setLayouts.resize(maxSet + 1, GetDescriptorCache().emptySetLayout);

  // For each set, build the DescriptorSetLayoutKey and get the layout
  for (const auto &setPair : shader->bindingInfos) {
    DescriptorSetLayoutKey layoutKey;
    layoutKey.flags = 0;
    layoutKey.bindings = setPair.second;
    layoutKey.bindingFlags = {};

    auto maxSetCount = context.deviceProperties.limits.maxBoundDescriptorSets;
    if (setPair.first >= maxSetCount) {
      return Error::Unexpected(
          std::format("Shader uses descriptor set {} which exceeds the device "
                      "limit of {}",
                      setPair.first, maxSetCount));
    }

    auto *layout = CHECK_RES(
        GetDescriptorCache().GetDescriptorSetLayout(layoutKey, context));
    setLayouts.at(setPair.first) = layout;
  }

  ERR_ASSERT(!setLayouts.empty());

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = setLayouts.size();
  pipelineLayoutInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

  {
    ZoneScopedN("Create pipeline layout");

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreatePipelineLayout(context.device, &pipelineLayoutInfo,
                                         GetAllocationCallbacks(),
                                         &pipelineLayout));
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    pipelineLayouts.emplace_back(pipelineLayout);
  }

  layouts.emplace(shader->getID(), PipelineLayout{
                                       .layout = pipelineLayout,
                                       .descriptorSetLayouts = setLayouts,
                                   });

  return PipelineLayout{
      .layout = pipelineLayout,
      .descriptorSetLayouts = setLayouts,
  };
}

auto GetPipelineCache() -> PipelineCache & {
  static PipelineCache cache{};

  return cache;
}

} // namespace Graphics