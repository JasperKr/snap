#include "descriptorState.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/FrameGraph/descriptorCache.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/object.hpp"
#include <cassert>

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables, readability-function-cognitive-complexity, cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

namespace Graphics {
inline auto BindBufferDesciptors(DescriptorKey &key, Ref<Shader> &shader,
                                 const BoundState &state, int setIndex)
    -> Error {

  auto iter = shader->reflection.bufferSlotsBySet.find(setIndex);
  if (iter == shader->reflection.bufferSlotsBySet.end()) {
    return Error::Success();
  }

  for (const auto &location : iter->second) {
    const auto [set, binding] = Utils::SlotToSetBinding(location);
    auto iter = state.userBoundBuffers.find(location);

    [[unlikely]]
    if (iter == state.userBoundBuffers.end() || !iter->second.first.isValid()) {
      continue;
    }

    const auto &buffer = iter->second;

    key.bindings.emplace_back(ResourceBinding{
        .binding = binding,
        .resource = buffer.first->getID(),
    });
  }

  return Error::Success();
}

inline auto BindAccelerationStructureDescriptors(DescriptorKey &key,
                                                 Ref<Shader> &shader,
                                                 const BoundState &state,
                                                 int setIndex) -> Error {
  if (!shader->reflection.accelerationStructureSlotsBySet.contains(setIndex)) {
    return Error::Success();
  }

  for (const auto &location :
       shader->reflection.accelerationStructureSlotsBySet.at(setIndex)) {
    const auto [set, binding] = Utils::SlotToSetBinding(location);
    assert(set == setIndex);
    auto iter = state.userBoundAccelerationStructures.find(location);

    [[unlikely]]
    if (iter == state.userBoundAccelerationStructures.end() ||
        !iter->second.first.isValid()) {
      continue;
    }

    key.bindings.emplace_back(ResourceBinding{
        .binding = binding,
        .resource = iter->second.first->getID(),
    });
  }

  return Error::Success();
}

// NOLINTNEXTLINE
inline auto BindTextureDescriptors(const GraphicsContext &context,
                                   VkPipelineStageFlags2 stage,
                                   DescriptorKey &key, Ref<Shader> &shader,
                                   int setIndex) -> Error {
  if (!shader->reflection.textureSlotsBySet.contains(setIndex)) {
    return Error::Success();
  }

  for (const auto &location :
       shader->reflection.textureSlotsBySet.at(setIndex)) {

    const auto &state = shader->GetState();

    assert(state.userBoundTextures.contains(location));
    auto iter = state.userBoundTextures.find(location);

    [[unlikely]]
    if (iter == state.userBoundTextures.end() ||
        !iter->second.first.isValid()) {
      continue;
    }

    const auto &pair = iter->second;
    const auto &texture = pair.first;
    const auto *samplerInfo = pair.second;
    const auto [set, binding] = Utils::SlotToSetBinding(location);

    key.bindings.emplace_back(ResourceBinding{
        .binding = binding,
        .resource = texture->getID(),
    });
  }

  return Error::Success();
}

inline auto BindGlobalsDescriptor(
    const GraphicsContext &context, DescriptorKey &key, Ref<Shader> &shader,
    int setIndex, Math::StackVector<uint32_t, 16> &dynamicOffsets) -> Error {
  ZoneScoped;

  [[unlikely]]
  if (!shader->reflection.hasGlobals) {
    return Error::Success();
  }

  const auto set = shader->reflection.globals.set;

  [[likely]]
  if (set != setIndex) {
    return Error::Success();
  }

  const auto binding = shader->reflection.globals.binding;

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);
  auto offset = buffer.GetOffset();
  dynamicOffsets.emplace_back(offset);

#if Enable_Snapshots
  Snapshot::CaptureEvent(Snapshot::StructuredBufferUploadEvent(
      buffer.GetBuffer()->handle, shader->reflection.globalBufferFormat));
#endif

  CHECK_ERR(buffer.Write(shader->globalUniforms));
  assert((buffer.GetBuffer()->usage & VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT) ==
         0);

  key.bindings.emplace_back(ResourceBinding{
      .binding = binding,
      .resource = buffer.GetBuffer()->getID(),
      .isDynamic = true,
  });

  return Error::Success();
}

inline auto AllocateDescriptorSet(const GraphicsContext &context,
                                  DescriptorKey &key, uint32_t setIndex,
                                  const VkDescriptorSetLayout &layout)
    -> Result<VkDescriptorSet> {
  ZoneScoped;

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = GetThreadContext().descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;
  VkDescriptorSet descriptorSet = nullptr;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(
        vkAllocateDescriptorSets(context.device, &allocInfo, &descriptorSet));
  }

  thread_local std::vector<VkWriteDescriptorSet> writeDescriptorSets;
  thread_local std::vector<VkDescriptorBufferInfo> bufferInfos;
  thread_local std::vector<VkDescriptorImageInfo> imageInfos;
  thread_local std::vector<VkWriteDescriptorSetAccelerationStructureKHR>
      accelStructInfos;
  thread_local std::vector<VkAccelerationStructureKHR> accelStructHandles;

  snap_defer(writeDescriptorSets.clear());
  snap_defer(bufferInfos.clear());
  snap_defer(imageInfos.clear());
  snap_defer(accelStructInfos.clear());
  snap_defer(accelStructHandles.clear());

  writeDescriptorSets.reserve(key.bindings.size());
  bufferInfos.reserve(key.bindings.size());
  imageInfos.reserve(key.bindings.size());
  accelStructInfos.reserve(key.bindings.size());
  accelStructHandles.reserve(key.bindings.size());

  auto &shader = RenderState::TopOfStack->shader;
  auto &state = shader->GetState();

  for (const auto &binding : key.bindings) {
    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptorSet;
    write.dstBinding = binding.binding;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;

    auto slot = Utils::SetBindingToSlot(setIndex, binding.binding);
    auto textureIter = state.userBoundTextures.find(slot);
    auto bufferIter = state.userBoundBuffers.find(slot);
    auto accelStructIter = state.userBoundAccelerationStructures.find(slot);

    if (binding.isDynamic) {
      auto &globalBuffer = GetGlobalUniformBuffer(context.frameIndex);

      ERR_ASSERT(globalBuffer.GetBuffer().isValid());

      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

      bufferInfos.emplace_back(VkDescriptorBufferInfo{
          .buffer = globalBuffer.GetBuffer()->handle,
          .offset = 0,
          .range = Utils::AlignUp(
              shader->reflection.globals.size,
              context.deviceProperties.limits.minUniformBufferOffsetAlignment),
      });

      write.pBufferInfo = &bufferInfos.back();
    } else if (textureIter != state.userBoundTextures.end()) {
      const auto &texture = textureIter->second.first;
      const auto *samplerInfo = textureIter->second.second;

      ERR_ASSERT(texture.isValid());

      if (samplerInfo->access == SLANG_RESOURCE_ACCESS_READ) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      } else if (samplerInfo->access == SLANG_RESOURCE_ACCESS_READ_WRITE ||
                 samplerInfo->access == SLANG_RESOURCE_ACCESS_WRITE) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      } else {
        return Error::Unexpected(
            "Unsupported sampler access type in descriptor set binding.");
      }

      imageInfos.emplace_back(VkDescriptorImageInfo{
          .sampler = texture->GetSampler(context),
          .imageView = texture->view,
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      });

      ERR_ASSERT(imageInfos.back().sampler != VK_NULL_HANDLE);
      ERR_ASSERT(imageInfos.back().imageView != VK_NULL_HANDLE);

      write.pImageInfo = &imageInfos.back();
    } else if (bufferIter != state.userBoundBuffers.end()) {
      const auto &buffer = bufferIter->second;

      ERR_ASSERT(buffer.first.isValid());

      if ((buffer.first->usage & VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT) != 0) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      } else if ((buffer.first->usage & VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT) !=
                 0) {
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      } else {
        return Error::Unexpected(
            "Buffer usage is not suitable for descriptor set binding.");
      }

      bufferInfos.emplace_back(VkDescriptorBufferInfo{
          .buffer = buffer.first->handle,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      });

      write.pBufferInfo = &bufferInfos.back();
    } else if (accelStructIter != state.userBoundAccelerationStructures.end()) {
      const auto &accelStruct = accelStructIter->second;

      ERR_ASSERT(accelStruct.first.isValid());

      write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

      VkWriteDescriptorSetAccelerationStructureKHR accelStructInfo = {};
      accelStructInfo.sType =
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
      accelStructInfo.accelerationStructureCount = 1;

      accelStructHandles.emplace_back(
          accelStruct.first->GetAccelerationStructure());
      accelStructInfo.pAccelerationStructures = &accelStructHandles.back();
      accelStructInfos.emplace_back(accelStructInfo);

      write.pNext = &accelStructInfos.back();
    } else {
      continue;
    }

    writeDescriptorSets.emplace_back(write);
  }

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    vkUpdateDescriptorSets(context.device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
  }

  assert(descriptorSet != VK_NULL_HANDLE);

  return descriptorSet;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto GetDescriptorSets(const GraphicsContext &context)
    -> Result<std::pair<Math::StackVector<VkDescriptorSet, 16>,
                        Math::StackVector<uint32_t, 16>>> {
  ZoneScoped;

  // PrintAlways("Binding descriptor sets.");

  auto &shader = RenderState::TopOfStack->shader;
  auto &state = shader->GetState();

  const VkPipelineStageFlags2 stageFlags =
      RenderState::TopOfStack->shader->combinedPipelineStages;

  struct BoundDescriptorSet {
    void *descriptorSet = nullptr;
    DescriptorKey descriptorKey{};
  };

  Math::StackVector<uint32_t, 16> dynamicOffsets{};
  Math::StackVector<VkDescriptorSet, 16> descriptorSets{};

  const auto &pipelineLayout =
      CHECK_RES(GetPipelineCache().GetPipelineLayout(context, shader.get()));
  auto &descriptorCache = GetDescriptorCache();
  Math::StackVector<uint32_t, 16> currentDynamicOffsets{};
  DescriptorKey key = {
      .bindings = {},
  };

  int setIndex = 0;
  for (const auto &layout : pipelineLayout.descriptorSetLayouts) {
    ZoneScopedN("BindDescriptorSets loop");

    key.bindings.clear();
    currentDynamicOffsets.fastclear();

    {
      ZoneScopedN("BindDescriptorSets Bind Resources");
      CHECK_ERR(BindBufferDesciptors(key, shader, state, setIndex));
      CHECK_ERR(
          BindTextureDescriptors(context, stageFlags, key, shader, setIndex));
      CHECK_ERR(
          BindAccelerationStructureDescriptors(key, shader, state, setIndex));
      CHECK_ERR(BindGlobalsDescriptor(context, key, shader, setIndex,
                                      currentDynamicOffsets));
    }

    VkDescriptorSet *entry = descriptorCache.descriptorSetCache.get(key);
    if (entry != nullptr) {
      descriptorSets.emplace_back(*entry);
      dynamicOffsets.insert(dynamicOffsets.end(), currentDynamicOffsets.begin(),
                            currentDynamicOffsets.end());
    } else {
      auto *descriptorSet =
          CHECK_RES(AllocateDescriptorSet(context, key, setIndex, layout));

      descriptorSets.emplace_back(descriptorSet);
      dynamicOffsets.insert(dynamicOffsets.end(), currentDynamicOffsets.begin(),
                            currentDynamicOffsets.end());

      descriptorCache.descriptorSetCache[key] = descriptorSet;
    }

    setIndex++;
  }

  return std::pair<Math::StackVector<VkDescriptorSet, 16>,
                   Math::StackVector<uint32_t, 16>>{descriptorSets,
                                                    dynamicOffsets};
}
} // namespace Graphics

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables, readability-function-cognitive-complexity, cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)