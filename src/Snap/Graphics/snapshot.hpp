// Graphics snapshot module
// Can be used to see what happened during a frame in-engine to help with profiling and debugging.

#pragma once

#include "Graphics/bufferformat.hpp"
#include "Graphics/renderState.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/object.hpp"
#include "Modules/timer.hpp"
#include "Modules/type.hpp"
#include <cstdint>
#include <imgui.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#define Enable_Snapshots 0

namespace Graphics::Snapshot {

enum class EventType : uint8_t {
  Unknown,

  Create_Buffer,
  Create_Texture,
  Create_Pipeline,
  Create_DescriptorSet,
  Create_Sampler,
  Create_ShaderModule,

  Destroy_Buffer,
  Destroy_Texture,
  Destroy_Pipeline,
  Destroy_DescriptorSet,
  Destroy_Sampler,
  Destroy_ShaderModule,

  Structured_Buffer_Upload,

  Upload_To_Buffer,
  Upload_To_Texture,

  Copy_Buffer_To_Buffer,
  Copy_Buffer_To_Texture,
  Copy_Texture_To_Buffer,
  Copy_Texture_To_Texture,

  Draw,
  DrawIndexed,
  DrawIndirect,
  DrawIndexedIndirect,

  Dispatch,
  DispatchIndirect,

  Set_Pipeline,
  Set_DescriptorSet,
  Set_VertexBuffer,
  Set_IndexBuffer,
  Set_PushConstants,

  Barrier,
  EndRendering,
  LayoutTransition,
};

const static Utils::EnumStringHelper<EventType> EventTypeStringHelper{{
    "Unknown",

    "Create_Buffer",
    "Create_Texture",
    "Create_Pipeline",
    "Create_DescriptorSet",
    "Create_Sampler",
    "Create_ShaderModule",

    "Destroy_Buffer",
    "Destroy_Texture",
    "Destroy_Pipeline",
    "Destroy_DescriptorSet",
    "Destroy_Sampler",
    "Destroy_ShaderModule",

    "Structured_Buffer_Upload",

    "Upload_To_Buffer",
    "Upload_To_Texture",

    "Copy_Buffer_To_Buffer",
    "Copy_Buffer_To_Texture",
    "Copy_Texture_To_Buffer",
    "Copy_Texture_To_Texture",

    "Draw",
    "DrawIndexed",
    "DrawIndirect",
    "DrawIndexedIndirect",

    "Dispatch",
    "DispatchIndirect",

    "Set_Pipeline",
    "Set_DescriptorSet",
    "Set_VertexBuffer",
    "Set_IndexBuffer",
    "Set_PushConstants",

    "Barrier",
    "EndRendering",
    "LayoutTransition",
}};

using Handle = void *;
using Timestamp = uint64_t;

struct Event {
  Event(const Event &) = default;
  Event(Event &&) = delete;
  auto operator=(const Event &) -> Event & = default;
  auto operator=(Event &&) -> Event & = delete;
  explicit Event(EventType type) : type(type), timestamp(Timer::GetTimeNS()) {}
  virtual ~Event();

  EventType type;
  Timestamp timestamp;

  auto DrawImGui(struct ThreadSnapshot const *parent) const -> void;
  virtual auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void;
};

struct GraphicsEvent {
  int renderState;

  GraphicsEvent();

  auto DrawStateImGui(struct ThreadSnapshot const *parent) const -> void;
};

// NOLINTBEGIN(bugprone-easily-swappable-parameters)

struct BufferCreateEvent : public Event {
  Handle bufferHandle{};
  Handle memoryHandle{};
  VkDeviceSize size{};
  uint32_t usage{};
  uint32_t properties{};

  BufferCreateEvent() : Event(EventType::Create_Buffer) {}

  BufferCreateEvent(Handle bufferHandle, Handle memoryHandle, VkDeviceSize size,
                    uint32_t usage, uint32_t properties)
      : Event(EventType::Create_Buffer), bufferHandle(bufferHandle),
        memoryHandle(memoryHandle), size(size), usage(usage),
        properties(properties) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct BufferDestroyEvent : public Event {
  Handle bufferHandle{};
  Handle memoryHandle{};

  BufferDestroyEvent() : Event(EventType::Destroy_Buffer) {}

  BufferDestroyEvent(Handle bufferHandle, Handle memoryHandle)
      : Event(EventType::Destroy_Buffer), bufferHandle(bufferHandle),
        memoryHandle(memoryHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct BufferUploadEvent : public Event {
  Handle bufferHandle{};
  Handle memoryHandle{};
  VkDeviceSize offset{};
  VkDeviceSize size{};
  std::vector<uint8_t> data;

  BufferUploadEvent() : Event(EventType::Upload_To_Buffer) {}
  BufferUploadEvent(Handle bufferHandle, Handle memoryHandle,
                    VkDeviceSize offset, VkDeviceSize size,
                    const std::vector<uint8_t> &data)
      : Event(EventType::Upload_To_Buffer), bufferHandle(bufferHandle),
        memoryHandle(memoryHandle), offset(offset), size(size), data(data) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct StructuredBufferUploadEvent : public Event {
  // If this event happens, it means we will be uploading to a structured buffer
  // Next event MUST be a BufferUploadEvent with the same bufferHandle.

  Handle bufferHandle{};
  BufferFormat format;
  BufferUploadEvent uploadEvent;
  bool hasAssociatedUploadEvent{false};

  StructuredBufferUploadEvent() : Event(EventType::Structured_Buffer_Upload) {}
  StructuredBufferUploadEvent(Handle bufferHandle, BufferFormat format)
      : Event(EventType::Structured_Buffer_Upload), bufferHandle(bufferHandle),
        format(std::move(format)) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct BufferCopyEvent : public Event {
  Handle srcBufferHandle{};
  Handle dstBufferHandle{};
  VkDeviceSize srcOffset{};
  VkDeviceSize dstOffset{};
  VkDeviceSize size{};

  BufferCopyEvent() : Event(EventType::Copy_Buffer_To_Buffer) {}
  BufferCopyEvent(Handle srcBufferHandle, Handle dstBufferHandle,
                  VkDeviceSize srcOffset, VkDeviceSize dstOffset,
                  VkDeviceSize size)
      : Event(EventType::Copy_Buffer_To_Buffer),
        srcBufferHandle(srcBufferHandle), dstBufferHandle(dstBufferHandle),
        srcOffset(srcOffset), dstOffset(dstOffset), size(size) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct TextureCreateEvent : public Event {
  Handle textureHandle{};
  Handle memoryHandle{};
  uint32_t width{};
  uint32_t height{};
  uint32_t depth{};
  uint32_t mipLevels{};
  uint32_t arrayLayers{};
  uint32_t format{};
  uint32_t usage{};
  uint32_t properties{};

  TextureCreateEvent() : Event(EventType::Create_Texture) {}
  TextureCreateEvent(Handle textureHandle, Handle memoryHandle, uint32_t width,
                     uint32_t height, uint32_t depth, uint32_t mipLevels,
                     uint32_t arrayLayers, uint32_t format, uint32_t usage,
                     uint32_t properties)
      : Event(EventType::Create_Texture), textureHandle(textureHandle),
        memoryHandle(memoryHandle), width(width), height(height), depth(depth),
        mipLevels(mipLevels), arrayLayers(arrayLayers), format(format),
        usage(usage), properties(properties) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct TextureDestroyEvent : public Event {
  Handle textureHandle{};
  Handle memoryHandle{};

  TextureDestroyEvent() : Event(EventType::Destroy_Texture) {}
  TextureDestroyEvent(Handle textureHandle, Handle memoryHandle)
      : Event(EventType::Destroy_Texture), textureHandle(textureHandle),
        memoryHandle(memoryHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct TextureUploadEvent : public Event {
  Handle textureHandle{};
  Handle memoryHandle{};
  uint32_t width{};
  uint32_t height{};
  uint32_t depth{};
  uint32_t mipLevel{};
  uint32_t arrayLayer{};
  uint32_t format{};
  VkDeviceSize dataSize{};

  TextureUploadEvent() : Event(EventType::Upload_To_Texture) {}
  TextureUploadEvent(Handle textureHandle, Handle memoryHandle, uint32_t width,
                     uint32_t height, uint32_t depth, uint32_t mipLevel,
                     uint32_t arrayLayer, uint32_t format,
                     VkDeviceSize dataSize)
      : Event(EventType::Upload_To_Texture), textureHandle(textureHandle),
        memoryHandle(memoryHandle), width(width), height(height), depth(depth),
        mipLevel(mipLevel), arrayLayer(arrayLayer), format(format),
        dataSize(dataSize) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct TextureCopyEvent : public Event {
  Handle srcTextureHandle{};
  Handle dstTextureHandle{};
  uint32_t srcWidth{};
  uint32_t srcHeight{};
  uint32_t srcDepth{};
  uint32_t srcMipLevel{};
  uint32_t srcArrayLayer{};
  uint32_t dstWidth{};
  uint32_t dstHeight{};
  uint32_t dstDepth{};
  uint32_t dstMipLevel{};
  uint32_t dstArrayLayer{};
  VkDeviceSize dataSize{};

  TextureCopyEvent() : Event(EventType::Copy_Texture_To_Texture) {}
  TextureCopyEvent(Handle srcTextureHandle, Handle dstTextureHandle,
                   uint32_t srcWidth, uint32_t srcHeight, uint32_t srcDepth,
                   uint32_t srcMipLevel, uint32_t srcArrayLayer,
                   uint32_t dstWidth, uint32_t dstHeight, uint32_t dstDepth,
                   uint32_t dstMipLevel, uint32_t dstArrayLayer,
                   VkDeviceSize dataSize)
      : Event(EventType::Copy_Texture_To_Texture),
        srcTextureHandle(srcTextureHandle), dstTextureHandle(dstTextureHandle),
        srcWidth(srcWidth), srcHeight(srcHeight), srcDepth(srcDepth),
        srcMipLevel(srcMipLevel), srcArrayLayer(srcArrayLayer),
        dstWidth(dstWidth), dstHeight(dstHeight), dstDepth(dstDepth),
        dstMipLevel(dstMipLevel), dstArrayLayer(dstArrayLayer),
        dataSize(dataSize) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct PipelineCreateEvent : public Event {
  Handle pipelineHandle{};
  uint32_t pipelineType{};

  PipelineCreateEvent() : Event(EventType::Create_Pipeline) {}
  PipelineCreateEvent(Handle pipelineHandle, uint32_t pipelineType)
      : Event(EventType::Create_Pipeline), pipelineHandle(pipelineHandle),
        pipelineType(pipelineType) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct PipelineDestroyEvent : public Event {
  Handle pipelineHandle{};

  PipelineDestroyEvent() : Event(EventType::Destroy_Pipeline) {}
  explicit PipelineDestroyEvent(Handle pipelineHandle)
      : Event(EventType::Destroy_Pipeline), pipelineHandle(pipelineHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DescriptorSetCreateEvent : public Event {
  Handle descriptorSetHandle{};

  DescriptorSetCreateEvent() : Event(EventType::Create_DescriptorSet) {}
  explicit DescriptorSetCreateEvent(Handle descriptorSetHandle)
      : Event(EventType::Create_DescriptorSet),
        descriptorSetHandle(descriptorSetHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DescriptorSetDestroyEvent : public Event {
  Handle descriptorSetHandle{};

  DescriptorSetDestroyEvent() : Event(EventType::Destroy_DescriptorSet) {}
  explicit DescriptorSetDestroyEvent(Handle descriptorSetHandle)
      : Event(EventType::Destroy_DescriptorSet),
        descriptorSetHandle(descriptorSetHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SamplerCreateEvent : public Event {
  Handle samplerHandle{};
  VkFilter magFilter{};
  VkFilter minFilter{};
  VkSamplerMipmapMode mipmapMode{};
  VkSamplerAddressMode addressModeU{};
  VkSamplerAddressMode addressModeV{};
  VkSamplerAddressMode addressModeW{};
  float mipLodBias{};
  bool anisotropyEnable{};
  float maxAnisotropy{};
  bool compareEnable{};
  VkCompareOp compareOp{};
  float minLod{};
  float maxLod{};
  VkBorderColor borderColor{};

  SamplerCreateEvent() : Event(EventType::Create_Sampler) {}
  SamplerCreateEvent(Handle samplerHandle, VkFilter magFilter,
                     VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
                     VkSamplerAddressMode addressModeU,
                     VkSamplerAddressMode addressModeV,
                     VkSamplerAddressMode addressModeW, float mipLodBias,
                     bool anisotropyEnable, float maxAnisotropy,
                     bool compareEnable, VkCompareOp compareOp, float minLod,
                     float maxLod, VkBorderColor borderColor)
      : Event(EventType::Create_Sampler), samplerHandle(samplerHandle),
        magFilter(magFilter), minFilter(minFilter), mipmapMode(mipmapMode),
        addressModeU(addressModeU), addressModeV(addressModeV),
        addressModeW(addressModeW), mipLodBias(mipLodBias),
        anisotropyEnable(anisotropyEnable), maxAnisotropy(maxAnisotropy),
        compareEnable(compareEnable), compareOp(compareOp), minLod(minLod),
        maxLod(maxLod), borderColor(borderColor) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SamplerDestroyEvent : public Event {
  Handle samplerHandle{};

  SamplerDestroyEvent() : Event(EventType::Destroy_Sampler) {}
  explicit SamplerDestroyEvent(Handle samplerHandle)
      : Event(EventType::Destroy_Sampler), samplerHandle(samplerHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct ShaderModuleCreateEvent : public Event {
  Handle shaderModuleHandle{};
  std::string moduleName;

  ShaderModuleCreateEvent() : Event(EventType::Create_ShaderModule) {}
  ShaderModuleCreateEvent(Handle shaderModuleHandle, std::string moduleName)
      : Event(EventType::Create_ShaderModule),
        shaderModuleHandle(shaderModuleHandle),
        moduleName(std::move(moduleName)) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct ShaderModuleDestroyEvent : public Event {
  Handle shaderModuleHandle{};

  ShaderModuleDestroyEvent() : Event(EventType::Destroy_ShaderModule) {}
  explicit ShaderModuleDestroyEvent(Handle shaderModuleHandle)
      : Event(EventType::Destroy_ShaderModule),
        shaderModuleHandle(shaderModuleHandle) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DrawEvent : public Event, public GraphicsEvent {
  uint32_t vertexCount{};
  uint32_t instanceCount{};
  uint32_t firstVertex{};
  uint32_t firstInstance{};

  DrawEvent() : Event(EventType::Draw) {}
  DrawEvent(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance)
      : Event(EventType::Draw), vertexCount(vertexCount),
        instanceCount(instanceCount), firstVertex(firstVertex),
        firstInstance(firstInstance) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DrawIndexedEvent : public Event, public GraphicsEvent {
  uint32_t indexCount{};
  uint32_t instanceCount{};
  uint32_t firstIndex{};
  int32_t vertexOffset{};
  uint32_t firstInstance{};

  DrawIndexedEvent() : Event(EventType::DrawIndexed) {}
  DrawIndexedEvent(uint32_t indexCount, uint32_t instanceCount,
                   uint32_t firstIndex, int32_t vertexOffset,
                   uint32_t firstInstance)
      : Event(EventType::DrawIndexed), indexCount(indexCount),
        instanceCount(instanceCount), firstIndex(firstIndex),
        vertexOffset(vertexOffset), firstInstance(firstInstance) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DrawIndirectEvent : public Event, public GraphicsEvent {
  Handle indirectBufferHandle{};
  VkDeviceSize offset{};
  uint32_t drawCount{};
  uint32_t stride{};

  DrawIndirectEvent() : Event(EventType::DrawIndirect) {}
  DrawIndirectEvent(Handle indirectBufferHandle, VkDeviceSize offset,
                    uint32_t drawCount, uint32_t stride)
      : Event(EventType::DrawIndirect),
        indirectBufferHandle(indirectBufferHandle), offset(offset),
        drawCount(drawCount), stride(stride) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DrawIndexedIndirectEvent : public Event, public GraphicsEvent {
  Handle indirectBufferHandle{};
  VkDeviceSize offset{};
  uint32_t drawCount{};
  uint32_t stride{};

  DrawIndexedIndirectEvent() : Event(EventType::DrawIndexedIndirect) {}
  DrawIndexedIndirectEvent(Handle indirectBufferHandle, VkDeviceSize offset,
                           uint32_t drawCount, uint32_t stride)
      : Event(EventType::DrawIndexedIndirect),
        indirectBufferHandle(indirectBufferHandle), offset(offset),
        drawCount(drawCount), stride(stride) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DispatchEvent : public Event, public GraphicsEvent {
  uint32_t groupCountX{};
  uint32_t groupCountY{};
  uint32_t groupCountZ{};

  DispatchEvent() : Event(EventType::Dispatch) {}
  DispatchEvent(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ)
      : Event(EventType::Dispatch), groupCountX(groupCountX),
        groupCountY(groupCountY), groupCountZ(groupCountZ) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct DispatchIndirectEvent : public Event, public GraphicsEvent {
  Handle indirectBufferHandle{};
  VkDeviceSize offset{};

  DispatchIndirectEvent() : Event(EventType::DispatchIndirect) {}
  DispatchIndirectEvent(Handle indirectBufferHandle, VkDeviceSize offset)
      : Event(EventType::DispatchIndirect),
        indirectBufferHandle(indirectBufferHandle), offset(offset) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SetPipelineEvent : public Event, public GraphicsEvent {
  Handle pipelineHandle{};
  uint32_t pipelineBindPoint{};

  SetPipelineEvent() : Event(EventType::Set_Pipeline) {}
  SetPipelineEvent(Handle pipelineHandle, uint32_t pipelineBindPoint)
      : Event(EventType::Set_Pipeline), pipelineHandle(pipelineHandle),
        pipelineBindPoint(pipelineBindPoint) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SetDescriptorSetEvent : public Event {
  Handle descriptorSetHandle{};
  uint32_t setIndex{};

  SetDescriptorSetEvent() : Event(EventType::Set_DescriptorSet) {}
  SetDescriptorSetEvent(Handle descriptorSetHandle, uint32_t setIndex)
      : Event(EventType::Set_DescriptorSet),
        descriptorSetHandle(descriptorSetHandle), setIndex(setIndex) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SetVertexBufferEvent : public Event {
  Handle bufferHandle{};
  VkDeviceSize offset{};

  SetVertexBufferEvent() : Event(EventType::Set_VertexBuffer) {}
  SetVertexBufferEvent(Handle bufferHandle, VkDeviceSize offset)
      : Event(EventType::Set_VertexBuffer), bufferHandle(bufferHandle),
        offset(offset) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SetIndexBufferEvent : public Event {
  Handle bufferHandle{};
  VkDeviceSize offset{};
  uint32_t indexType{};

  SetIndexBufferEvent() : Event(EventType::Set_IndexBuffer) {}
  SetIndexBufferEvent(Handle bufferHandle, VkDeviceSize offset,
                      uint32_t indexType)
      : Event(EventType::Set_IndexBuffer), bufferHandle(bufferHandle),
        offset(offset), indexType(indexType) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct SetPushConstantsEvent : public Event {
  uint32_t layoutHandle{};
  uint32_t stageFlags{};
  uint32_t offset{};
  uint32_t size{};
  std::vector<uint8_t> data;
  BufferFormat pushConstantFormat;

  SetPushConstantsEvent() : Event(EventType::Set_PushConstants) {}
  SetPushConstantsEvent(uint32_t layoutHandle, uint32_t stageFlags,
                        uint32_t offset, uint32_t size,
                        const std::vector<uint8_t> &data,
                        BufferFormat pushConstantFormat)
      : Event(EventType::Set_PushConstants), layoutHandle(layoutHandle),
        stageFlags(stageFlags), offset(offset), size(size), data(data),
        pushConstantFormat(std::move(pushConstantFormat)) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

// struct BarrierEvent : public Event {
//   Barrier::ResourceSync sync{};
//   ObjectID resourceId{};

//   BarrierEvent() : Event(EventType::Barrier) {}
//   explicit BarrierEvent(const Barrier::ResourceSync &sync, ObjectID resourceId)
//       : Event(EventType::Barrier), sync(sync), resourceId(resourceId) {}

//   auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
//       -> void override;
// };

struct LayoutTransitionEvent : public Event {
  VkImageLayout srcLayout{};
  VkImageLayout dstLayout{};

  VkAccessFlags2 srcAccessMask{};
  VkAccessFlags2 dstAccessMask{};

  VkPipelineStageFlags2 srcStageMask{};
  VkPipelineStageFlags2 dstStageMask{};

  LayoutTransitionEvent() : Event(EventType::LayoutTransition) {}
  explicit LayoutTransitionEvent(VkImageLayout srcLayout,
                                 VkImageLayout dstLayout,
                                 VkAccessFlags2 srcAccessMask,
                                 VkAccessFlags2 dstAccessMask,
                                 VkPipelineStageFlags2 srcStageMask,
                                 VkPipelineStageFlags2 dstStageMask)
      : srcLayout(srcLayout), dstLayout(dstLayout),
        srcAccessMask(srcAccessMask), dstAccessMask(dstAccessMask),
        srcStageMask(srcStageMask), dstStageMask(dstStageMask),
        Event(EventType::LayoutTransition) {}

  auto DrawVariantImGui(struct ThreadSnapshot const *parent) const
      -> void override;
};

struct EndRenderingEvent : public Event {
  EndRenderingEvent() : Event(EventType::EndRendering) {}
};

constexpr VkShaderStageFlagBits allRTStages =
    static_cast<VkShaderStageFlagBits>(
        static_cast<uint32_t>(VK_SHADER_STAGE_ANY_HIT_BIT_KHR) |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR);

inline auto HexToImU32(uint32_t hex) -> ImU32 {
  uint32_t result = hex;

  // NOLINTBEGIN
  uint8_t *parts = reinterpret_cast<uint8_t *>(&result);
  uint8_t temp = parts[0];
  parts[0] = parts[2];
  parts[2] = temp;
  parts[3] = 0xFF;
  // NOLINTEND

  return result;
}

// clang-format off
const std::unordered_map<VkShaderStageFlagBits,
                         std::pair<ImColor, const char *>>
    ShaderStageUiInfo{
        {VK_SHADER_STAGE_FRAGMENT_BIT, {HexToImU32(0x1b50ba), "Fragment"}},
        {VK_SHADER_STAGE_VERTEX_BIT, {HexToImU32(0x1bba1b), "Vertex"}},
        {VK_SHADER_STAGE_COMPUTE_BIT, {HexToImU32(0xc9c320), "Compute"}},
        {allRTStages, {HexToImU32(0xc92023), "Raytracing"}},
    };

const std::unordered_map<VkPipelineStageFlagBits2,
                         std::pair<ImColor, const char *>>
    PipelineStageUiInfo{
        {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, {HexToImU32(0x158715), "Vertex input"}},
        {VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, {HexToImU32(0x1bba1b), "Vertex shader"}},
        {VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, {HexToImU32(0x1b50ba), "Fragment shader"}},
        {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, {HexToImU32(0x18449b), "Early fragment test"}},
        {VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, {HexToImU32(0x205cd6), "Late fragment test"}},
        {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, {HexToImU32(0x20d6c0), "Color attachment output"}},
        {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, {HexToImU32(0xc9c320), "Compute shader"}},
        {VK_PIPELINE_STAGE_2_TRANSFER_BIT, {HexToImU32(0xb420d6), "Transfer"}},
    };
// clang-format on

auto DrawPipelineStage(VkPipelineStageFlagBits2 stage);
auto DrawPipelineStages(VkPipelineStageFlagBits2 stages);

auto DrawShaderStage(VkShaderStageFlagBits stage);
auto DrawShaderStages(VkShaderStageFlagBits stages);

static const Type ThreadSnapshotType = Type("ThreadSnapshot");

struct ThreadSnapshot : Object, Identifiable {
  std::vector<std::shared_ptr<Event>> events;
  std::vector<RenderState::State> renderStates;

  uint64_t threadId;
  std::string threadName;
  bool active{false};

  [[nodiscard]] auto GetName() const -> std::string_view {
    if (!threadName.empty()) {
      return threadName;
    }

    return "Unnamed Thread";
  }

  [[nodiscard]] auto Copy() const -> Ref<ThreadSnapshot> {
    return Ref<ThreadSnapshot>::Make(events, renderStates, threadId, threadName,
                                     false);
  }

  static auto GetType() -> Type const * { return &ThreadSnapshotType; }
  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return GetType();
  }

  ThreadSnapshot(std::vector<std::shared_ptr<Event>> events,
                 std::vector<RenderState::State> renderStates,
                 uint64_t threadId, std::string threadName, bool active)
      : events(std::move(events)), renderStates(std::move(renderStates)),
        threadId(threadId), threadName(std::move(threadName)), active(active) {}
};

// NOLINTEND(bugprone-easily-swappable-parameters)

auto GetCurrentSnapshot() -> ThreadSnapshot *;

template <typename T> auto CaptureEvent(const T &event) -> bool {
#if Enable_Snapshots
  auto *currentSnapshot = GetCurrentSnapshot();

  if (currentSnapshot == nullptr) {
    return false;
  }

  auto index = currentSnapshot->events.size();
  currentSnapshot->events.emplace_back(std::make_unique<T>(event));

  const auto &newEvent = currentSnapshot->events[index];

  if (index > 0) {
    const auto &back = currentSnapshot->events[index - 1];

    if (back->type == EventType::Structured_Buffer_Upload &&
        newEvent->type == EventType::Upload_To_Buffer) {

      // NOLINTBEGIN(cppcoreguidelines-pro-type-static-cast-downcast)
      auto *structuredEvent =
          static_cast<StructuredBufferUploadEvent *>(back.get());
      auto *bufferUploadEvent =
          static_cast<BufferUploadEvent *>(newEvent.get());
      // NOLINTEND(cppcoreguidelines-pro-type-static-cast-downcast)

      structuredEvent->uploadEvent = *bufferUploadEvent;
      structuredEvent->hasAssociatedUploadEvent = true;
    }
  }
#endif

  return true;
}

auto Save(const ThreadSnapshot &snapshot, const std::string &filename) -> void;
auto Load(const std::string &filename) -> ThreadSnapshot;
auto StartSnapshot() -> void;
auto EndSnapshot() -> void;
auto RenderSnapshot(const ThreadSnapshot &snapshot) -> void;
auto Update() -> void;

} // namespace Graphics::Snapshot