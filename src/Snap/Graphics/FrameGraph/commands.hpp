#pragma once

#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/error.hpp"
#include "Modules/stackVector.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

// 65K events per frame should be more than enough
// Loading bistro in a single frame takes < 15K events, so it should be fine.
using CommandID = uint16_t;
static constexpr uint16_t InvalidCommandID = UINT16_MAX;

// We have a virtual command buffer per-thread
// Each command buffer stores the thread's recorded commands,
// later, they will be combined in to a final large frame virtual command buffer
// a DAG will be built and barriers will be inserted.

// A virtual command buffer will be one-time use and have some userdata;
// I want to support different queue families so a vcmd must store which queue family it will be queued to.
// These command buffers are NOT thread-safe. Must either be externally synchronised, or just have 1 copy per-thread.
// only one vk command buffer will be associated per vcmd, or even less than one if one can span multiple (for example, when queue families match)

// This means commands do not need to reference the command buffer at recording time.

enum class CommandType : uint8_t {
  vkCmdDraw,
  vkCmdDrawIndexed,
  vkCmdDrawIndirect,
  vkCmdDrawIndexedIndirect,
  vkCmdDispatch,
  vkCmdDispatchIndirect,
  vkCmdBlitImage,

  vkCmdCopyBuffer,
  vkCmdCopyImage,
  vkCmdCopyBufferToImage,
  vkCmdCopyImageToBuffer,

  mipmapTexture,
  vkCmdFillBuffer,

  vkCmdBuildAccelerationStructuresKHR,
  vkCmdCopyAccelerationStructureKHR,
  vkCmdResetQueryPool,
  vkCmdWriteAccelerationStructuresPropertiesKHR,

  vkCmdClearAttachments,
  vkCmdPipelineBarrier2,

  renderPass,
};

static const Utils::EnumStringHelper<CommandType> CommandTypeEnumHelper{{
    "vkCmdDraw",
    "vkCmdDrawIndexed",
    "vkCmdDrawIndirect",
    "vkCmdDrawIndexedIndirect",
    "vkCmdDispatch",
    "vkCmdDispatchIndirect",
    "vkCmdBlitImage",

    "vkCmdCopyBuffer",
    "vkCmdCopyImage",
    "vkCmdCopyBufferToImage",
    "vkCmdCopyImageToBuffer",

    "mipmapTexture",
    "vkCmdFillBuffer",

    "vkCmdBuildAccelerationStructuresKHR",
    "vkCmdCopyAccelerationStructureKHR",
    "vkCmdResetQueryPool",
    "vkCmdWriteAccelerationStructuresPropertiesKHR",

    "vkCmdClearAttachments",
    "vkCmdPipelineBarrier2",

    "renderPass",
}};

struct GraphState {
  VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
  VkFrontFace frontFace = VK_FRONT_FACE_MAX_ENUM;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_MAX_ENUM;
  VkBool32 stencilTestEnable = 0;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_MAX_ENUM;
  VkViewport viewport{};
  VkRect2D scissor{};

  mutable bool dirty = true;

  std::string currentDebugMarker;
  std::optional<Color> currentDebugMarkerColor;
  Math::StackVector<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS>
      colorBlendEquations;

  Ref<Shader> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
  Math::StackVector<RenderState::RenderTarget, MAX_COLOR_ATTACHMENTS>
      colorAttachments;
  RenderState::RenderTarget depthStencilAttachment;
  bool hasDepthStencilAttachment = false;

  std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;

  // Fields down from here are not taken into account for hashing,
  // and are used purely for copying to per-draw state
  std::vector<char> pushConstants;

  std::vector<VkBuffer> vertexBuffers;
  std::vector<VkDeviceSize> vertexBufferOffsets;
  VkBuffer indexBuffer = VK_NULL_HANDLE;
  VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
  VkDeviceSize indexBufferOffset{};

  mutable uint64_t hash{};

  // Incremented each time the state is modified
  mutable uint64_t generation = 0;

  void MarkUpdated() {
    generation++;
    dirty = true;
  }

  auto GetHash() const -> uint64_t;

  auto operator==(const GraphState &other) const -> bool {
    [[likely]]
    if (generation == other.generation) { // quick equal
      return true;
    }

    if (colorAttachments != other.colorAttachments) {
      return false;
    }

    if (shader != other.shader) {
      return false;
    }

    if (stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology ||
        bindPoint != other.bindPoint) {
      return false;
    }

    if (hasDepthStencilAttachment != other.hasDepthStencilAttachment) {
      return false;
    }

    if (hasDepthStencilAttachment) {
      if (depthStencilAttachment != other.depthStencilAttachment) {
        return false;
      }
    }

    return true;
  }

  auto ToString() const -> std::string;
};

struct GraphStateHash {
  auto operator()(const GraphState &state) const -> uint64_t {
    return state.GetHash();
  };
};

struct CommandStateManager {
  static std::unordered_map<GraphState, uint32_t, GraphStateHash> StateToIndex;
  static std::vector<GraphState> States;
  static uint32_t CurrentStateID;
};

// NOLINTBEGIN(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTBEGIN(hicpp-explicit-conversions)

struct Callable {
  virtual ~Callable() = default;
  virtual auto Call(VkCommandBuffer cmdBuffer) const -> Error = 0;

  // Determines if to call Begin or End rendering. (though dispatches still require Begin)
  bool requiresRendering{};
};

struct ImageSubresource {
  VkImage image;

  uint16_t layerStart;
  uint16_t layerCount;
  uint16_t mipStart;
  uint16_t mipCount;

  ImageSubresource(const Ref<Texture> &texture);
  ImageSubresource(VkImage image, uint16_t layerStart, uint16_t layerCount,
                   uint16_t mipStart, uint16_t mipCount)
      : image(image), layerCount(layerCount), layerStart(layerStart),
        mipCount(mipCount), mipStart(mipStart) {}
  ImageSubresource(VkImage image, VkImageSubresourceRange range)
      : image(image), layerCount(range.layerCount),
        layerStart(range.baseArrayLayer), mipCount(range.levelCount),
        mipStart(range.baseMipLevel) {}

  [[nodiscard]] auto Overlaps(const ImageSubresource &other) const -> bool {
    return image == other.image &&
           layerStart < (other.layerStart + other.layerCount) &&
           other.layerStart < (layerStart + layerCount) &&
           mipStart < (other.mipStart + other.mipCount) &&
           other.mipStart < (mipStart + mipCount);
  }
};

struct VulkanResource {
  enum class ResourceType : uint8_t {
    Image,
    Buffer,
    AccelerationStructure,
  };

  ResourceType type;

  union {
    ImageSubresource image;
    VkBuffer buffer;
    VkAccelerationStructureKHR accelerationStructure;
  };

  VulkanResource(const ImageSubresource &image)
      : type(ResourceType::Image), image(image) {}

  VulkanResource(VkBuffer buffer)
      : type(ResourceType::Buffer), buffer(buffer) {}

  VulkanResource(VkAccelerationStructureKHR accelerationStructure)
      : type(ResourceType::AccelerationStructure),
        accelerationStructure(accelerationStructure) {}

  [[nodiscard]]
  auto Overlaps(const VulkanResource &other) const -> bool {
    if (type != other.type) {
      return false;
    }

    switch (type) {
    case ResourceType::Image:
      return image.Overlaps(other.image);

    case ResourceType::Buffer:
      return buffer == other.buffer;

    case ResourceType::AccelerationStructure:
      return accelerationStructure == other.accelerationStructure;

    default:
      assert(false && "Unreachable");
    }
  }

  auto operator<(const VulkanResource &other) const -> bool {
    if (type != other.type) {
      return type < other.type;
    }

    if (type == ResourceType::AccelerationStructure) {
      return accelerationStructure < other.accelerationStructure;
    }

    if (type == ResourceType::Buffer) {
      return buffer < other.buffer;
    }

    return std::tie(image.image, image.layerStart, image.layerCount,
                    image.mipStart, image.mipCount) <
           std::tie(other.image.image, other.image.layerStart,
                    other.image.layerCount, other.image.mipStart,
                    other.image.mipCount);
  }

  [[nodiscard]] static auto Equals(const VulkanResource &resource,
                                   const VulkanResource &other) -> bool {
    if (resource.type != other.type) {
      return false;
    }

    switch (resource.type) {
    case ResourceType::AccelerationStructure:
      return resource.accelerationStructure == other.accelerationStructure;
    case ResourceType::Buffer:
      return resource.buffer == other.buffer;
    case ResourceType::Image:
      return resource.image.image == other.image.image &&
             resource.image.layerStart == other.image.layerStart &&
             resource.image.layerCount == other.image.layerCount &&
             resource.image.mipStart == other.image.mipStart &&
             resource.image.mipCount == other.image.mipCount;
    }
  }

  [[nodiscard]] auto ToString() const -> std::string {
    switch (type) {
    case ResourceType::AccelerationStructure:
      return std::format("Acceleration structure {}",
                         (void *)accelerationStructure);
    case ResourceType::Buffer:
      return std::format("Buffer {}", (void *)buffer);
    case ResourceType::Image:
      return std::format("Image {}", (void *)image.image);
    }
  }

  auto operator==(const VulkanResource &other) const noexcept -> bool {
    if (type != other.type) {
      return false;
    }

    switch (type) {
    case ResourceType::AccelerationStructure:
      return accelerationStructure == other.accelerationStructure;
    case ResourceType::Buffer:
      return buffer == other.buffer;
    case ResourceType::Image:
      return image.image == other.image.image;
    }

    return false;
  }
};

struct VulkanResourceHash {
  auto operator()(const VulkanResource &resource) const noexcept -> uint64_t {
    Hash::Hasher hasher;
    hasher.Add(static_cast<uint32_t>(resource.type));

    switch (resource.type) {
    case VulkanResource::ResourceType::Image:
      hasher.Add(resource.image.image);
      break;
    case VulkanResource::ResourceType::Buffer:
      hasher.Add(resource.buffer);
      break;
    case VulkanResource::ResourceType::AccelerationStructure:
      hasher.Add(resource.accelerationStructure);
      break;
    default:
      assert(false && "Unreachable");
    }

    return hasher.Get();
  }
};

struct BoundResources {
  std::vector<VulkanResource> reads;
  std::vector<VulkanResource> writes;
};

struct BoundResource {
  VulkanResource resource;
  VkAccessFlags2 access;
  VkPipelineStageFlags2 pipelines;

  [[nodiscard]]
  auto Overlaps(const BoundResource &other) const -> bool {
    return resource.Overlaps(other.resource);
  }

  [[nodiscard]]
  auto Overlaps(const VulkanResource &other) const -> bool {
    return resource.Overlaps(other);
  }
};

struct LoadOpConfig {
  std::vector<VkAttachmentLoadOp> loadOps;
  VkAttachmentLoadOp depthStencilLoadOp;

  static auto FromGraphState(const GraphState &graphState, bool forceLoadOpLoad)
      -> LoadOpConfig;
};

struct DrawState {
  std::vector<BoundResource> boundImages;
  std::vector<BoundResource> boundBuffers;
  std::vector<BoundResource> boundASs;

  std::vector<BoundResource> colorAttachments;
  std::optional<BoundResource> depthStencilAttachment;

  std::vector<VkBuffer> vertexBuffers;
  VkBuffer indexBuffer = VK_NULL_HANDLE;

  std::vector<VkDeviceSize> vertexBufferOffsets;
  VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
  VkDeviceSize indexBufferOffset{};

  // Copied from graph state.
  std::vector<char> pushConstants;

  Math::StackVector<VkDescriptorSet, 16> descriptorSets; // NOLINT
  Math::StackVector<uint32_t, 16> dynamicOffsets;        // NOLINT

  uint32_t stateID = UINT32_MAX;

  [[nodiscard]] auto GetStateFor(const VulkanResource &resource,
                                 CommandType type) const
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;

  [[nodiscard]] auto GetReadStateFor(const VulkanResource &resource,
                                     CommandType type) const
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;

  [[nodiscard]] auto GetWriteStateFor(const VulkanResource &resource,
                                      CommandType type) const
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;

  auto Apply(const GraphicsContext &context, VkCommandBuffer cmdBuffer,
             const LoadOpConfig *loadConfig) const -> Error;

  auto Initialize(const GraphicsContext &context, CommandType type) -> Error;

  [[nodiscard]] auto GetGraphState() const -> GraphState {
    return CommandStateManager::States.at(stateID);
  }
};

namespace Args {

struct VkCmdDraw : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDraw;

  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;

  VkCmdDraw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance)
      : vertexCount(vertexCount), instanceCount(instanceCount),
        firstVertex(firstVertex), firstInstance(firstInstance) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex,
              firstInstance);
    return {};
  }
};

struct VkCmdDrawIndexed : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndexed;

  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;

  VkCmdDrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                   uint32_t firstIndex, int32_t vertexOffset,
                   uint32_t firstInstance)
      : indexCount(indexCount), instanceCount(instanceCount),
        firstIndex(firstIndex), vertexOffset(vertexOffset),
        firstInstance(firstInstance) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex,
                     vertexOffset, firstInstance);
    return {};
  }
};

struct VkCmdDrawIndirect : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndirect;

  VkBuffer buffer; // TODO: Add these to reads.
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  VkCmdDrawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                    uint32_t stride)
      : buffer(buffer), offset(offset), drawCount(drawCount), stride(stride) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndirect(cmdBuffer, buffer, offset, drawCount, stride);
    return {};
  }
};

struct VkCmdDrawIndexedIndirect : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDrawIndexedIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;
  uint32_t drawCount;
  uint32_t stride;

  VkCmdDrawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset,
                           uint32_t drawCount, uint32_t stride)
      : buffer(buffer), offset(offset), drawCount(drawCount), stride(stride) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offset, drawCount, stride);
    return {};
  }
};

struct VkCmdDispatch : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDispatch;

  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;

  VkCmdDispatch(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ)
      : groupCountX(groupCountX), groupCountY(groupCountY),
        groupCountZ(groupCountZ) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
    return {};
  }
};

struct VkCmdDispatchIndirect : Callable, DrawState, BoundResources {
  static const CommandType type = CommandType::vkCmdDispatchIndirect;

  VkBuffer buffer;
  VkDeviceSize offset;

  VkCmdDispatchIndirect(VkBuffer buffer, VkDeviceSize offset)
      : buffer(buffer), offset(offset) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDispatchIndirect(cmdBuffer, buffer, offset);
    return {};
  }
};

struct VkCmdBlitImage : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdBlitImage;

  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  std::vector<VkImageBlit> regions;
  VkFilter filter;
  std::vector<VulkanResource> srcResources;
  std::vector<VulkanResource> dstResources;

  VkCmdBlitImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageBlit *pRegions,
                 VkFilter filter)
      : srcImage(srcImage), srcImageLayout(srcImageLayout), dstImage(dstImage),
        dstImageLayout(dstImageLayout), filter(filter),
        regions(pRegions, pRegions + regionCount) {
    srcResources.reserve(regionCount);
    dstResources.reserve(regionCount);

    for (auto &region : regions) {
      srcResources.emplace_back(ImageSubresource(
          srcImage, region.srcSubresource.baseArrayLayer,
          region.srcSubresource.layerCount, region.srcSubresource.mipLevel, 1));

      dstResources.emplace_back(ImageSubresource(
          dstImage, region.dstSubresource.baseArrayLayer,
          region.dstSubresource.layerCount, region.dstSubresource.mipLevel, 1));
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data(), filter);
    return {};
  }
};

struct VkCmdPushConstants {
  VkPipelineLayout layout;
  VkShaderStageFlags stageFlags;
  uint32_t offset;
  std::vector<char> values;

  VkCmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                     uint32_t offset, uint32_t size, const void *pValues)
      : layout(layout), stageFlags(stageFlags), offset(offset),
        values(static_cast<const char *>(pValues),
               static_cast<const char *>(pValues) + size) {}
};

struct VkCmdCopyBuffer : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyBuffer;

  VkBuffer srcBuffer;
  VkBuffer dstBuffer;
  std::vector<VkBufferCopy> regions;

  VkCmdCopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                  const VkBufferCopy *pRegions)
      : srcBuffer(srcBuffer), dstBuffer(dstBuffer),
        regions(pRegions, pRegions + regionCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, regions.size(),
                    regions.data());
    return {};
  }
};

struct VkCmdCopyImage : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyImage;

  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  std::vector<VkImageCopy> regions;

  std::vector<VulkanResource> srcResources;
  std::vector<VulkanResource> dstResources;

  VkCmdCopyImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageCopy *pRegions)
      : srcImage(srcImage), srcImageLayout(srcImageLayout), dstImage(dstImage),
        dstImageLayout(dstImageLayout),
        regions(pRegions, pRegions + regionCount) {
    srcResources.reserve(regionCount);
    dstResources.reserve(regionCount);

    for (auto &region : regions) {
      srcResources.emplace_back(ImageSubresource(
          srcImage, region.srcSubresource.baseArrayLayer,
          region.srcSubresource.layerCount, region.srcSubresource.mipLevel, 1));

      dstResources.emplace_back(ImageSubresource(
          dstImage, region.dstSubresource.baseArrayLayer,
          region.dstSubresource.layerCount, region.dstSubresource.mipLevel, 1));
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data());
    return {};
  }
};

struct VkCmdCopyBufferToImage : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyBufferToImage;

  VkBuffer srcBuffer;
  VkImage dstImage;
  VkImageLayout dstImageLayout;
  std::vector<VkBufferImageCopy> regions;
  std::vector<VulkanResource> dstResources;

  VkCmdCopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage,
                         VkImageLayout dstImageLayout, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions)
      : srcBuffer(srcBuffer), dstImage(dstImage),
        dstImageLayout(dstImageLayout),
        regions(pRegions, pRegions + regionCount) {
    dstResources.reserve(regionCount);

    for (auto &region : regions) {
      dstResources.emplace_back(
          ImageSubresource(dstImage, region.imageSubresource.baseArrayLayer,
                           region.imageSubresource.layerCount,
                           region.imageSubresource.mipLevel, 1));
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage, dstImageLayout,
                           regions.size(), regions.data());
    return {};
  }
};

struct VkCmdCopyImageToBuffer : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdCopyImageToBuffer;

  VkImage srcImage;
  VkImageLayout srcImageLayout;
  VkBuffer dstBuffer;
  std::vector<VkBufferImageCopy> regions;
  std::vector<VulkanResource> srcResources;

  VkCmdCopyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout,
                         VkBuffer dstBuffer, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions)
      : srcImage(srcImage), srcImageLayout(srcImageLayout),
        dstBuffer(dstBuffer), regions(pRegions, pRegions + regionCount) {
    srcResources.reserve(regionCount);

    for (auto &region : regions) {
      srcResources.emplace_back(
          ImageSubresource(srcImage, region.imageSubresource.baseArrayLayer,
                           region.imageSubresource.layerCount,
                           region.imageSubresource.mipLevel, 1));
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, dstBuffer,
                           regions.size(), regions.data());
    return {};
  }
};

struct MipmapTexture : Callable, BoundResources {
  static const CommandType type = CommandType::mipmapTexture;

  Texture *texture;

  MipmapTexture(Texture *texture) : texture(texture) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    return texture->GenerateMipmaps(*GetCurrentGraphicsContext(), cmdBuffer);
  }
};

struct VkCmdFillBuffer : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdFillBuffer;

  VkBuffer dstBuffer;
  VkDeviceSize dstOffset;
  VkDeviceSize size;
  uint32_t data;

  VkCmdFillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size,
                  uint32_t data)
      : dstBuffer(dstBuffer), dstOffset(dstOffset), size(size), data(data) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdFillBuffer(cmdBuffer, dstBuffer, dstOffset, size, data);
    return {};
  }
};

struct VkCmdBuildAccelerationStructuresKHR : Callable, BoundResources {
  static const CommandType type =
      CommandType::vkCmdBuildAccelerationStructuresKHR;

  uint32_t infoCount;

  std::vector<VkAccelerationStructureGeometryKHR> geometries;
  std::vector<VkAccelerationStructureBuildGeometryInfoKHR> infos;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

  VkCmdBuildAccelerationStructuresKHR(
      uint32_t infoCount,
      const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
      const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos,
      const std::vector<VkBuffer> &reads, const std::vector<VkBuffer> &writes)
      : infoCount(infoCount), infos(pInfos, pInfos + infoCount), // NOLINT
        buildRangeInfos(infoCount) {

    size_t geometryCount = 0;

    for (const auto &info : infos) {
      geometryCount += info.geometryCount;
    }

    this->reads.reserve(reads.size());
    this->writes.reserve(writes.size());

    for (auto *read : reads) {
      this->reads.emplace_back(read);
    }

    for (auto *write : writes) {
      this->writes.emplace_back(write);
    }

    geometries.reserve(geometryCount);

    for (size_t i = 0; i < infoCount; ++i) {
      assert(infos[i].pNext == nullptr);
      assert(infos[i].ppGeometries == nullptr);

      const size_t offset = geometries.size();

      for (uint32_t j = 0; j < infos[i].geometryCount; ++j) {
        geometries.push_back(infos[i].pGeometries[j]);
      }

      buildRangeInfos[i] = *ppBuildRangeInfos[i];
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR *>
        ppBuildRangeInfos;
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> infos2 = infos;
    ppBuildRangeInfos.reserve(buildRangeInfos.size());

    for (const auto &range : buildRangeInfos) {
      ppBuildRangeInfos.push_back(&range);
    }

    size_t offset = 0;
    for (size_t i = 0; i < infoCount; ++i) {
      infos2.at(i).pGeometries = geometries.data() + offset;
      offset += infos2.at(i).geometryCount;
    }

    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, infoCount, infos2.data(),
                                        ppBuildRangeInfos.data());
    return {};
  }
};

struct VkCmdCopyAccelerationStructureKHR : Callable {
  static const CommandType type =
      CommandType::vkCmdCopyAccelerationStructureKHR;

  VkCopyAccelerationStructureInfoKHR structureInfo;

  VkCmdCopyAccelerationStructureKHR(
      const VkCopyAccelerationStructureInfoKHR *pInfo)
      : structureInfo(*pInfo) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &structureInfo);
    return {};
  }
};

struct VkCmdResetQueryPool : Callable {
  static const CommandType type = CommandType::vkCmdResetQueryPool;

  VkQueryPool queryPool;
  uint32_t firstQuery;
  uint32_t queryCount;

  VkCmdResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery,
                      uint32_t queryCount)
      : queryPool(queryPool), firstQuery(firstQuery), queryCount(queryCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdResetQueryPool(cmdBuffer, queryPool, firstQuery, queryCount);
    return {};
  }
};

struct VkCmdWriteAccelerationStructuresPropertiesKHR : Callable {
  static const CommandType type =
      CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR;

  std::vector<VkAccelerationStructureKHR> accelerationStructures;
  VkQueryType queryType;
  VkQueryPool queryPool;
  uint32_t firstQuery;

  VkCmdWriteAccelerationStructuresPropertiesKHR(
      uint32_t accelerationStructureCount,
      const VkAccelerationStructureKHR *pAccelerationStructures,
      VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
      : accelerationStructures(pAccelerationStructures,
                               pAccelerationStructures +
                                   accelerationStructureCount),
        queryType(queryType), queryPool(queryPool), firstQuery(firstQuery) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdWriteAccelerationStructuresPropertiesKHR(
        cmdBuffer, accelerationStructures.size(), accelerationStructures.data(),
        queryType, queryPool, firstQuery);
    return {};
  }
};

struct VkCmdBindIndexBuffer {
  VkBuffer buffer;
  VkDeviceSize offset;
  VkIndexType indexType;

  VkCmdBindIndexBuffer(VkBuffer buffer, VkDeviceSize offset,
                       VkIndexType indexType)
      : buffer(buffer), offset(offset), indexType(indexType) {}
};

struct VkCmdBindVertexBuffers {
  uint32_t firstBinding;
  std::vector<VkBuffer> buffers;
  std::vector<VkDeviceSize> offsets;

  VkCmdBindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount,
                         const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
      : firstBinding(firstBinding), buffers(pBuffers, pBuffers + bindingCount),

        offsets(pOffsets, pOffsets + bindingCount) {}
};

struct VkCmdSetVertexInputEXT {
  std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;

  VkCmdSetVertexInputEXT(
      uint32_t vertexBindingDescriptionCount,
      const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
      uint32_t vertexAttributeDescriptionCount,
      const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
      : bindingDescriptions(pVertexBindingDescriptions,
                            pVertexBindingDescriptions +
                                vertexBindingDescriptionCount),
        attributeDescriptions(pVertexAttributeDescriptions,
                              pVertexAttributeDescriptions +
                                  vertexAttributeDescriptionCount) {}
};

struct VkCmdBindPipeline {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipeline pipeline;

  VkCmdBindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
      : pipelineBindPoint(pipelineBindPoint), pipeline(pipeline) {}
};

struct VkCmdBindDescriptorSets {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipelineLayout layout;
  uint32_t firstSet;
  std::vector<VkDescriptorSet> descriptorSets;
  std::vector<uint32_t> dynamicOffsets;

  VkCmdBindDescriptorSets(VkPipelineBindPoint pipelineBindPoint,
                          VkPipelineLayout layout, uint32_t firstSet,
                          uint32_t descriptorSetCount,
                          const VkDescriptorSet *pDescriptorSets,
                          uint32_t dynamicOffsetCount,
                          const uint32_t *pDynamicOffsets)
      : pipelineBindPoint(pipelineBindPoint), layout(layout),
        firstSet(firstSet),
        descriptorSets(pDescriptorSets, pDescriptorSets + descriptorSetCount),

        dynamicOffsets(pDynamicOffsets, pDynamicOffsets + dynamicOffsetCount) {}
};

struct VkCmdSetViewport {
  uint32_t firstViewport;
  std::vector<VkViewport> viewports;

  VkCmdSetViewport(uint32_t firstViewport, uint32_t viewportCount,
                   const VkViewport *pViewports)
      : firstViewport(firstViewport),
        viewports(pViewports, pViewports + viewportCount) {}
};

struct VkCmdSetScissor {
  uint32_t firstScissor;
  std::vector<VkRect2D> scissors;

  VkCmdSetScissor(uint32_t firstScissor, uint32_t scissorCount,
                  const VkRect2D *pScissors)
      : firstScissor(firstScissor),
        scissors(pScissors, pScissors + scissorCount) {}
};

struct VkCmdSetDepthTestEnable {
  VkBool32 depthTestEnable;

  VkCmdSetDepthTestEnable(VkBool32 depthTestEnable)
      : depthTestEnable(depthTestEnable) {}
};

struct VkCmdSetDepthWriteEnable {
  VkBool32 depthWriteEnable;

  VkCmdSetDepthWriteEnable(VkBool32 depthWriteEnable)
      : depthWriteEnable(depthWriteEnable) {}
};

struct VkCmdSetDepthCompareOp {
  VkCompareOp depthCompareOp;

  VkCmdSetDepthCompareOp(VkCompareOp depthCompareOp)
      : depthCompareOp(depthCompareOp) {}
};

struct VkCmdSetColorBlendEquationEXT {
  uint32_t firstAttachment;
  std::vector<VkColorBlendEquationEXT> equations;

  VkCmdSetColorBlendEquationEXT(
      uint32_t firstAttachment, uint32_t attachmentCount,
      const VkColorBlendEquationEXT *pColorBlendEquations)
      : firstAttachment(firstAttachment),
        equations(pColorBlendEquations,
                  pColorBlendEquations + attachmentCount) {}
};

struct VkCmdSetCullMode {
  VkCullModeFlags cullMode;

  VkCmdSetCullMode(VkCullModeFlags cullMode) : cullMode(cullMode) {}
};

struct VkCmdSetFrontFace {
  VkFrontFace frontFace;

  VkCmdSetFrontFace(VkFrontFace frontFace) : frontFace(frontFace) {}
};

struct VkCmdClearAttachments : Callable, BoundResources, DrawState {
  static const CommandType type = CommandType::vkCmdClearAttachments;

  std::vector<VkClearAttachment> attachments;
  std::vector<VkClearRect> rects;

  VkCmdClearAttachments(uint32_t attachmentCount,
                        const VkClearAttachment *pAttachments,
                        uint32_t rectCount, const VkClearRect *pRects)
      : attachments(pAttachments, pAttachments + attachmentCount),
        rects(pRects, pRects + rectCount) {
    requiresRendering = true;
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdClearAttachments(cmdBuffer, attachments.size(), attachments.data(),
                          rects.size(), rects.data());
    return {};
  }
};

struct VkCmdBeginDebugUtilsLabelEXT {
  VkDebugUtilsLabelEXT labelInfo;

  VkCmdBeginDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT *pLabelInfo)
      : labelInfo(*pLabelInfo) {}
};

struct VkCmdEndDebugUtilsLabelEXT {};

struct VkCmdInsertDebugUtilsLabelEXT {
  VkDebugUtilsLabelEXT labelInfo;

  VkCmdInsertDebugUtilsLabelEXT(const VkDebugUtilsLabelEXT *pLabelInfo)
      : labelInfo(*pLabelInfo) {}
};

struct VkCmdPipelineBarrier2 : Callable, BoundResources {
  static const CommandType type = CommandType::vkCmdPipelineBarrier2;

  VkDependencyInfo dependencyInfo;
  std::vector<VkMemoryBarrier2> memoryBarriers;
  std::vector<VkBufferMemoryBarrier2> bufferMemoryBarriers;
  std::vector<VkImageMemoryBarrier2> imageMemoryBarriers;

  VkCmdPipelineBarrier2(const VkDependencyInfo *pDependencyInfo)
      : dependencyInfo(*pDependencyInfo) {
    if (pDependencyInfo->pMemoryBarriers != nullptr) {
      memoryBarriers.assign(pDependencyInfo->pMemoryBarriers,
                            pDependencyInfo->pMemoryBarriers +
                                pDependencyInfo->memoryBarrierCount);
    }

    if (pDependencyInfo->pBufferMemoryBarriers != nullptr) {
      bufferMemoryBarriers.assign(
          pDependencyInfo->pBufferMemoryBarriers,
          pDependencyInfo->pBufferMemoryBarriers +
              pDependencyInfo->bufferMemoryBarrierCount);
    }

    if (pDependencyInfo->pImageMemoryBarriers != nullptr) {
      imageMemoryBarriers.assign(pDependencyInfo->pImageMemoryBarriers,
                                 pDependencyInfo->pImageMemoryBarriers +
                                     pDependencyInfo->imageMemoryBarrierCount);
    }
  }

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    VkDependencyInfo tempDepInfo = dependencyInfo;

    tempDepInfo.pMemoryBarriers =
        memoryBarriers.empty() ? nullptr : memoryBarriers.data();
    tempDepInfo.pBufferMemoryBarriers =
        bufferMemoryBarriers.empty() ? nullptr : bufferMemoryBarriers.data();
    tempDepInfo.pImageMemoryBarriers =
        imageMemoryBarriers.empty() ? nullptr : imageMemoryBarriers.data();

    vkCmdPipelineBarrier2(cmdBuffer, &tempDepInfo);
    return {};
  }
};

// NOLINTEND(hicpp-explicit-conversions)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTEND(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
} // namespace Args

using RenderPassCommands =
    std::variant<Args::VkCmdDraw, Args::VkCmdDrawIndexed,
                 Args::VkCmdDrawIndirect, Args::VkCmdDrawIndexedIndirect>;

struct RenderPass : BoundResources {
  static const CommandType type = CommandType::renderPass;

  uint32_t stateID;
  std::vector<CommandID> commands;
};

template <typename T, typename... Ts>
auto get_if_derived(std::variant<Ts...> &variant) -> T * {
  T *result = nullptr;

  std::visit(
      [&](auto &value) -> auto {
        using U = std::remove_cvref_t<decltype(value)>;

        if constexpr (std::derived_from<U, T>) {
          result = &value;
        }
      },
      variant);

  return result;
}

template <typename T, typename... Ts>
auto get_if_derived(const std::variant<Ts...> &variant) -> T const * {
  T const *result = nullptr;

  std::visit(
      [&](auto &value) -> auto {
        using U = std::remove_cvref_t<decltype(value)>;

        if constexpr (std::derived_from<U, T>) {
          result = &value;
        }
      },
      variant);

  return result;
}

using ArgVariants = std::variant<
    Args::VkCmdDraw, Args::VkCmdDrawIndexed, Args::VkCmdDrawIndirect,
    Args::VkCmdDrawIndexedIndirect, Args::VkCmdDispatch,
    Args::VkCmdDispatchIndirect, Args::VkCmdBlitImage, Args::VkCmdCopyBuffer,
    Args::VkCmdCopyImage, Args::VkCmdCopyBufferToImage,
    Args::VkCmdCopyImageToBuffer, Args::MipmapTexture, Args::VkCmdFillBuffer,
    Args::VkCmdBuildAccelerationStructuresKHR,
    Args::VkCmdCopyAccelerationStructureKHR, Args::VkCmdResetQueryPool,
    Args::VkCmdWriteAccelerationStructuresPropertiesKHR,
    Args::VkCmdClearAttachments, Args::VkCmdPipelineBarrier2, RenderPass>;

struct Command {
  CommandID id = InvalidCommandID;

  // Also defined in framegraph.cpp as InvalidDepth
  CommandID level = UINT16_MAX;

  ArgVariants data;

  explicit Command(ArgVariants params) : data(std::move(params)) {}

  [[nodiscard]] auto GetType() const -> CommandType {
    return std::visit(
        [](const auto &current) -> CommandType { return current.type; }, data);
  }

  [[nodiscard]] auto GetDrawState() -> DrawState * {
    return get_if_derived<DrawState>(data);
  }

  [[nodiscard]] auto GetDrawState() const -> DrawState const * {
    return get_if_derived<DrawState>(data);
  }
};

struct BufferStateUpdate {
  VkBuffer buffer;
  ResourceState state;
  CommandID time;
};

struct ImageStateUpdate {
  VkImage image;
  ResourceState state;
  CommandID time;
};

auto GetReads(const Command &command) -> const std::vector<VulkanResource> &;
auto GetWrites(const Command &command) -> const std::vector<VulkanResource> &;

struct VirtualCommandBuffer {
  friend struct FrameGraph;

  auto Append(const VirtualCommandBuffer &buffer) -> Error {
    ERR_ASSERT(buffer.queueFamily == buffer.queueFamily);

    commands.append_range(buffer.commands);

    return {};
  }

  auto Draw(const Args::VkCmdDraw &arguments) -> Error;
  auto DrawIndexed(const Args::VkCmdDrawIndexed &arguments) -> Error;
  auto DrawIndirect(const Args::VkCmdDrawIndirect &arguments) -> Error;
  auto DrawIndexedIndirect(const Args::VkCmdDrawIndexedIndirect &arguments)
      -> Error;
  auto Dispatch(const Args::VkCmdDispatch &arguments) -> Error;
  auto DispatchIndirect(const Args::VkCmdDispatchIndirect &arguments) -> Error;
  auto BlitImage(const Args::VkCmdBlitImage &arguments) -> Error;
  auto PushConstants(const Args::VkCmdPushConstants &arguments) -> void;
  auto CopyBuffer(const Args::VkCmdCopyBuffer &arguments) -> Error;
  auto CopyImage(const Args::VkCmdCopyImage &arguments) -> Error;
  auto CopyBufferToImage(const Args::VkCmdCopyBufferToImage &arguments)
      -> Error;
  auto CopyImageToBuffer(const Args::VkCmdCopyImageToBuffer &arguments)
      -> Error;

  // Custom command, workaround for the barrier system working on an image-based granularity, not range based
  auto MipmapTexture(const Args::MipmapTexture &arguments) -> Error;
  auto FillBuffer(const Args::VkCmdFillBuffer &arguments) -> Error;
  auto BuildAccelerationStructuresKHR(
      const Args::VkCmdBuildAccelerationStructuresKHR &arguments) -> Error;
  auto CopyAccelerationStructureKHR(
      const Args::VkCmdCopyAccelerationStructureKHR &arguments) -> Error;
  auto ResetQueryPool(const Args::VkCmdResetQueryPool &arguments) -> Error;
  auto WriteAccelerationStructuresPropertiesKHR(
      const Args::VkCmdWriteAccelerationStructuresPropertiesKHR &arguments)
      -> Error;
  auto BindIndexBuffer(const Args::VkCmdBindIndexBuffer &arguments) -> void;
  auto BindVertexBuffers(const Args::VkCmdBindVertexBuffers &arguments) -> void;
  auto SetVertexInputEXT(const Args::VkCmdSetVertexInputEXT &arguments) -> void;
  auto BindPipeline(const Args::VkCmdBindPipeline &arguments) -> void;
  auto BindDescriptorSets(const Args::VkCmdBindDescriptorSets &arguments)
      -> void;
  auto SetViewport(const Args::VkCmdSetViewport &arguments) -> void;
  auto SetScissor(const Args::VkCmdSetScissor &arguments) -> void;
  auto SetDepthTestEnable(const Args::VkCmdSetDepthTestEnable &arguments)
      -> void;
  auto SetDepthWriteEnable(const Args::VkCmdSetDepthWriteEnable &arguments)
      -> void;
  auto SetDepthCompareOp(const Args::VkCmdSetDepthCompareOp &arguments) -> void;
  auto
  SetColorBlendEquationEXT(const Args::VkCmdSetColorBlendEquationEXT &arguments)
      -> void;
  auto SetCullMode(const Args::VkCmdSetCullMode &arguments) -> void;
  auto SetFrontFace(const Args::VkCmdSetFrontFace &arguments) -> void;
  auto ClearAttachments(const Args::VkCmdClearAttachments &arguments) -> Error;
  auto
  BeginDebugUtilsLabelEXT(const Args::VkCmdBeginDebugUtilsLabelEXT &arguments)
      -> void;
  auto EndDebugUtilsLabelEXT(const Args::VkCmdEndDebugUtilsLabelEXT &arguments)
      -> void;
  auto
  InsertDebugUtilsLabelEXT(const Args::VkCmdInsertDebugUtilsLabelEXT &arguments)
      -> void;
  auto PipelineBarrier2(const Args::VkCmdPipelineBarrier2 &arguments) -> Error;

  [[nodiscard]] auto GetQueueFamily() const -> uint32_t { return queueFamily; }

  auto Reset() -> void;

  auto GetStateID() -> uint32_t {
    auto iter = CommandStateManager::StateToIndex.find(currentState);

    if (iter == CommandStateManager::StateToIndex.end()) {
      auto newIdx = CommandStateManager::States.size();

      CommandStateManager::StateToIndex[currentState] = newIdx;
      CommandStateManager::States.emplace_back(currentState);

      return newIdx;
    }

    return iter->second;
  }

  auto GetGraphState() -> GraphState & { return currentState; }

protected:
  // NOLINTBEGIN

  uint64_t time;

  auto AddCommand(const Command &command) -> Error;

  std::vector<Command> commands;

  uint32_t queueFamily;

  GraphState currentState;

  // NOLINTEND
};

auto CreateCommandBuffer() -> VirtualCommandBuffer;
auto ResetCommandBuffer(VirtualCommandBuffer &buffer) -> void;

} // namespace Graphics