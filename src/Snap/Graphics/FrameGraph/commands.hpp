#pragma once

#include "Graphics/FrameGraph/resourceUsage.hpp"
#include "Graphics/FrameGraph/virtualState.hpp"
#include "Graphics/FrameGraph/vulkanResource.hpp"
#include "Graphics/graphics.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/error.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <public/tracy/Tracy.hpp>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

using CommandID = uint16_t;
using CommandIndex = uint32_t;

static const CommandID InvalidCommandID = UINT16_MAX;

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
  vkCmdDraw, // If this is no longer the first element, update in VirtualCommandBuffer::Initialize
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

  renderPass, // If this is no longer the last element, update in VirtualCommandBuffer::Initialize
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

// NOLINTBEGIN(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTBEGIN(hicpp-explicit-conversions)

struct Callable {
  virtual ~Callable() = default;
  virtual auto Call(VkCommandBuffer cmdBuffer) const -> Error = 0;
};

struct BoundResources {
  std::vector<VulkanResource> reads;
  std::vector<VulkanResource> writes;
  std::unordered_map<VulkanResource,
                     std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                     VulkanResourceHash<true>>
      accesses;
};

struct DrawState {
  VkCullModeFlags cullMode = VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
  VkFrontFace frontFace = VK_FRONT_FACE_MAX_ENUM;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_MAX_ENUM;
  VkViewport viewport{};
  VkRect2D scissor{};

  std::vector<BoundResource> boundImages;
  std::vector<BoundResource> boundBuffers;
  std::vector<BoundResource> boundASs;

  std::vector<VkBuffer> vertexBuffers;
  VkBuffer indexBuffer = VK_NULL_HANDLE;

  std::vector<VkDeviceSize> vertexBufferOffsets;
  VkIndexType indexType = VK_INDEX_TYPE_MAX_ENUM;
  VkDeviceSize indexBufferOffset{};

  std::vector<VkVertexInputBindingDescription2EXT> bindingDescriptions;
  std::vector<VkVertexInputAttributeDescription2EXT> attributeDescriptions;

  uint32_t uboAddress;

  auto GetAccesses(
      std::unordered_map<VulkanResource,
                         std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                         VulkanResourceHash<true>> &accesses) const -> void;

  auto Apply(const GraphicsContext &context, const struct RenderPass &parent,
             VkCommandBuffer cmdBuffer) const -> Error;

  auto Initialize(CommandType type) -> Error;
};

namespace Args {

struct VkCmdDraw : Callable, DrawState, BoundResources {
  VkCmdDraw() = default;

  static const CommandType type = CommandType::vkCmdDraw;

  uint32_t vertexCount{};
  uint32_t instanceCount{};
  uint32_t firstVertex{};
  uint32_t firstInstance{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDraw(cmdBuffer, vertexCount, instanceCount, firstVertex,
              firstInstance);
    return {};
  }
};

struct VkCmdDrawIndexed : Callable, DrawState, BoundResources {
  VkCmdDrawIndexed() = default;

  static const CommandType type = CommandType::vkCmdDrawIndexed;

  uint32_t indexCount{};
  uint32_t instanceCount{};
  uint32_t firstIndex{};
  int32_t vertexOffset{};
  uint32_t firstInstance{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex,
                     vertexOffset, firstInstance);
    return {};
  }
};

struct VkCmdDrawIndirect : Callable, DrawState, BoundResources {
  VkCmdDrawIndirect() = default;

  static const CommandType type = CommandType::vkCmdDrawIndirect;

  VkBuffer buffer{}; // TODO: Add these to reads.
  VkDeviceSize offset{};
  uint32_t drawCount{};
  uint32_t stride{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndirect(cmdBuffer, buffer, offset, drawCount, stride);
    return {};
  }
};

struct VkCmdDrawIndexedIndirect : Callable, DrawState, BoundResources {
  VkCmdDrawIndexedIndirect() = default;

  static const CommandType type = CommandType::vkCmdDrawIndexedIndirect;

  VkBuffer buffer{};
  VkDeviceSize offset{};
  uint32_t drawCount{};
  uint32_t stride{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDrawIndexedIndirect(cmdBuffer, buffer, offset, drawCount, stride);
    return {};
  }
};

struct VkCmdDispatch : Callable, DrawState, BoundResources {
  VkCmdDispatch() = default;

  static const CommandType type = CommandType::vkCmdDispatch;

  uint32_t groupCountX{};
  uint32_t groupCountY{};
  uint32_t groupCountZ{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDispatch(cmdBuffer, groupCountX, groupCountY, groupCountZ);
    return {};
  }
};

struct VkCmdDispatchIndirect : Callable, DrawState, BoundResources {
  VkCmdDispatchIndirect() = default;

  static const CommandType type = CommandType::vkCmdDispatchIndirect;

  VkBuffer buffer{};
  VkDeviceSize offset{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdDispatchIndirect(cmdBuffer, buffer, offset);
    return {};
  }
};

struct VkCmdBlitImage : Callable, BoundResources {
  VkCmdBlitImage() = default;

  static const CommandType type = CommandType::vkCmdBlitImage;

  VkImage srcImage{};
  VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  VkImage dstImage{};
  VkImageLayout dstImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  std::vector<VkImageBlit> regions;
  VkFilter filter = VK_FILTER_MAX_ENUM;
  std::vector<VulkanResource> srcResources;
  std::vector<VulkanResource> dstResources;

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdBlitImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data(), filter);
    return {};
  }
};

struct VkCmdPushConstants {
  VkCmdPushConstants() = default;

  VkShaderStageFlags stageFlags{};
  uint32_t offset{};
  std::vector<char> values;
};

struct VkCmdCopyBuffer : Callable, BoundResources {
  VkCmdCopyBuffer() = default;

  static const CommandType type = CommandType::vkCmdCopyBuffer;

  VkBuffer srcBuffer{};
  VkBuffer dstBuffer{};
  std::vector<VkBufferCopy> regions;

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, regions.size(),
                    regions.data());
    return {};
  }
};

struct VkCmdCopyImage : Callable, BoundResources {
  VkCmdCopyImage() = default;

  static const CommandType type = CommandType::vkCmdCopyImage;

  VkImage srcImage{};
  VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  VkImage dstImage{};
  VkImageLayout dstImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  std::vector<VkImageCopy> regions;

  std::vector<VulkanResource> srcResources;
  std::vector<VulkanResource> dstResources;

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyImage(cmdBuffer, srcImage, srcImageLayout, dstImage,
                   dstImageLayout, regions.size(), regions.data());
    return {};
  }
};

struct VkCmdCopyBufferToImage : Callable, BoundResources {
  VkCmdCopyBufferToImage() = default;

  static const CommandType type = CommandType::vkCmdCopyBufferToImage;

  VkBuffer srcBuffer{};
  VkImage dstImage{};
  VkImageLayout dstImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  std::vector<VkBufferImageCopy> regions;
  std::vector<VulkanResource> dstResources;

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyBufferToImage(cmdBuffer, srcBuffer, dstImage, dstImageLayout,
                           regions.size(), regions.data());
    return {};
  }
};

struct VkCmdCopyImageToBuffer : Callable, BoundResources {
  VkCmdCopyImageToBuffer() = default;

  static const CommandType type = CommandType::vkCmdCopyImageToBuffer;

  VkImage srcImage{};
  VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
  VkBuffer dstBuffer{};
  std::vector<VkBufferImageCopy> regions;
  std::vector<VulkanResource> srcResources;

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyImageToBuffer(cmdBuffer, srcImage, srcImageLayout, dstBuffer,
                           regions.size(), regions.data());
    return {};
  }
};

struct MipmapTexture : Callable, BoundResources {
  MipmapTexture() = default;

  static const CommandType type = CommandType::mipmapTexture;

  Texture *texture{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    return texture->GenerateMipmaps(*GetCurrentGraphicsContext(), cmdBuffer);
  }
};

struct VkCmdFillBuffer : Callable, BoundResources {
  VkCmdFillBuffer() = default;

  static const CommandType type = CommandType::vkCmdFillBuffer;

  VkBuffer dstBuffer{};
  VkDeviceSize dstOffset{};
  VkDeviceSize size{};
  uint32_t data{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdFillBuffer(cmdBuffer, dstBuffer, dstOffset, size, data);
    return {};
  }
};

struct VkCmdBuildAccelerationStructuresKHR : Callable, BoundResources {
  VkCmdBuildAccelerationStructuresKHR() = default;

  static const CommandType type =
      CommandType::vkCmdBuildAccelerationStructuresKHR;

  uint32_t infoCount{};

  std::vector<VkAccelerationStructureGeometryKHR> geometries;
  std::vector<VkAccelerationStructureBuildGeometryInfoKHR> infos;
  std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;

  std::vector<VkBuffer> bufferReads;
  std::vector<VkBuffer> bufferWrites;

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
  VkCmdCopyAccelerationStructureKHR() = default;

  static const CommandType type =
      CommandType::vkCmdCopyAccelerationStructureKHR;

  VkCopyAccelerationStructureInfoKHR structureInfo{};

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdCopyAccelerationStructureKHR(cmdBuffer, &structureInfo);
    return {};
  }
};

struct VkCmdResetQueryPool : Callable {
  VkCmdResetQueryPool() = default;

  static const CommandType type = CommandType::vkCmdResetQueryPool;

  VkQueryPool queryPool{};
  uint32_t firstQuery{};
  uint32_t queryCount{};

  VkCmdResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery,
                      uint32_t queryCount)
      : queryPool(queryPool), firstQuery(firstQuery), queryCount(queryCount) {}

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    vkCmdResetQueryPool(cmdBuffer, queryPool, firstQuery, queryCount);
    return {};
  }
};

struct VkCmdWriteAccelerationStructuresPropertiesKHR : Callable {
  VkCmdWriteAccelerationStructuresPropertiesKHR() = default;

  static const CommandType type =
      CommandType::vkCmdWriteAccelerationStructuresPropertiesKHR;

  std::vector<VkAccelerationStructureKHR> accelerationStructures;
  VkQueryType queryType = VK_QUERY_TYPE_MAX_ENUM;
  VkQueryPool queryPool{};
  uint32_t firstQuery{};

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

struct VkCmdBindPipeline {
  VkPipelineBindPoint pipelineBindPoint;
  VkPipeline pipeline;

  VkCmdBindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline)
      : pipelineBindPoint(pipelineBindPoint), pipeline(pipeline) {}
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

struct VkCmdSetCullMode {
  VkCullModeFlags cullMode;

  VkCmdSetCullMode(VkCullModeFlags cullMode) : cullMode(cullMode) {}
};

struct VkCmdSetFrontFace {
  VkFrontFace frontFace;

  VkCmdSetFrontFace(VkFrontFace frontFace) : frontFace(frontFace) {}
};

struct VkCmdClearAttachments : Callable, BoundResources, DrawState {
  VkCmdClearAttachments() = default;

  static const CommandType type = CommandType::vkCmdClearAttachments;

  std::vector<VkClearAttachment> attachments;
  std::vector<VkClearRect> rects;

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
  VkCmdPipelineBarrier2() = default;

  static const CommandType type = CommandType::vkCmdPipelineBarrier2;

  std::vector<VkMemoryBarrier2> memoryBarriers;
  std::vector<VkBufferMemoryBarrier2> bufferMemoryBarriers;
  std::vector<VkImageMemoryBarrier2> imageMemoryBarriers;

  auto Call(VkCommandBuffer cmdBuffer) const -> Error override {
    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    dep.pMemoryBarriers = memoryBarriers.data();
    dep.pBufferMemoryBarriers = bufferMemoryBarriers.data();
    dep.pImageMemoryBarriers = imageMemoryBarriers.data();

    dep.memoryBarrierCount = memoryBarriers.size();
    dep.bufferMemoryBarrierCount = bufferMemoryBarriers.size();
    dep.imageMemoryBarrierCount = imageMemoryBarriers.size();

    vkCmdPipelineBarrier2(cmdBuffer, &dep);
    return {};
  }
};

// NOLINTEND(hicpp-explicit-conversions)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTEND(cppcoreguidelines-special-member-functions, hicpp-special-member-functions)
} // namespace Args

auto GetRequiresRendering(CommandType type) -> bool;

using RenderPassCommands =
    std::variant<Args::VkCmdDraw, Args::VkCmdDrawIndexed,
                 Args::VkCmdDrawIndirect, Args::VkCmdDrawIndexedIndirect>;

struct RenderPass : BoundResources {
  static const CommandType type = CommandType::renderPass;

  std::vector<CommandID> commands;
  VirtualRenderState state;
};

template <typename T, typename... Ts>
auto get_if_derived(std::variant<Ts...> &variant) -> T * {
  T *result = nullptr;

  assert(!variant.valueless_by_exception());

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
  CommandIndex id = UINT16_MAX;
  CommandIndex parentID = UINT16_MAX;
  uint16_t uniqueID = UINT16_MAX;

  // Also defined in framegraph.cpp as InvalidDepth
  uint16_t level = UINT16_MAX;

  ArgVariants data;
  CommandType type;

  explicit Command(ArgVariants params) : data(std::move(params)) {
    type = std::visit(
        [](const auto &current) -> CommandType { return current.type; }, data);
  }

  void UpdateType() {
    type = std::visit(
        [](const auto &current) -> CommandType { return current.type; }, data);
  }

  [[nodiscard]] auto GetType() const -> CommandType { return type; }

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
auto GetResourceAccesses(
    const Command &command,
    std::unordered_map<VulkanResource,
                       std::pair<VkAccessFlags2, VkPipelineStageFlags2>,
                       VulkanResourceHash<true>> &accesses) -> void;

struct ReadyInfo {
  std::condition_variable condition_variable;
  std::mutex mutex;
  bool ready = false;

  void Notify() {
    {
      std::unique_lock<std::mutex> lock(mutex);
      ready = true;
    }

    condition_variable.notify_one();
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mutex);
    condition_variable.wait(lock, [this]() -> bool { return ready; });
    ready = false;
  }
};

struct VCmdThreadInfo {
  std::thread thread;

  // Thread swaps between reading and editing A / B based on "currentWorkingIsA"
  std::vector<uint16_t> commandsQueue;
  std::vector<uint16_t> queuedCommands; // Local to worker thread
  std::vector<uint16_t> commandsQueueBacklog;
  std::mutex commandsQueueMutex;

  ReadyInfo mainToWorker;
  ReadyInfo workerToMain;

  std::atomic<bool> shutdown = false;
  bool started = false;

  VirtualCommandBuffer *parent;

  void Shutdown() {
    shutdown.store(true);
    mainToWorker.Notify();
  }
  void Initialize(VirtualCommandBuffer *vcmd);

  void AddCommand(uint16_t commandID);

  void StartCommandWriting() {
    TracyMessageL("Started");
    started = true;

    {
      std::lock_guard<std::mutex> lock(commandsQueueMutex);
      commandsQueue.append_range(commandsQueueBacklog);
    }
    commandsQueueBacklog.clear();

    mainToWorker.Notify();
  }

  void FinalizeCommandWriting() {
    workerToMain.Wait();

    TracyMessageL("Ended");
    started = false;
  }

  static void ThreadMain(VirtualCommandBuffer *vcmd);
};

struct VirtualCommandBuffer { // NOLINT
  friend struct FrameGraph;

  auto Append(const VirtualCommandBuffer &buffer) -> Error {
    ERR_ASSERT(buffer.queueFamily == buffer.queueFamily);

    commands.append_range(buffer.commands);

    return {};
  }

  auto Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
            uint32_t firstInstance) -> Error;
  auto DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                   uint32_t firstIndex, int32_t vertexOffset,
                   uint32_t firstInstance) -> Error;
  auto DrawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount,
                    uint32_t stride) -> Error;
  auto DrawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset,
                           uint32_t drawCount, uint32_t stride) -> Error;
  auto Dispatch(uint32_t groupCountX, uint32_t groupCountY,
                uint32_t groupCountZ) -> Error;
  auto DispatchIndirect(VkBuffer buffer, VkDeviceSize offsets) -> Error;
  auto BlitImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageBlit *pRegions,
                 VkFilter filter) -> Error;
  auto CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount,
                  const VkBufferCopy *pRegions) -> Error;
  auto CopyImage(VkImage srcImage, VkImageLayout srcImageLayout,
                 VkImage dstImage, VkImageLayout dstImageLayout,
                 uint32_t regionCount, const VkImageCopy *pRegions) -> Error;
  auto CopyBufferToImage(VkBuffer srcBuffer, VkImage dstImage,
                         VkImageLayout dstImageLayout, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions) -> Error;
  auto CopyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout,
                         VkBuffer dstBuffer, uint32_t regionCount,
                         const VkBufferImageCopy *pRegions) -> Error;

  // Custom command, workaround for the barrier system working on an image-based granularity, not range based
  auto MipmapTexture(Texture *texture) -> Error;
  auto FillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size,
                  uint32_t data) -> Error;
  auto BuildAccelerationStructuresKHR(
      uint32_t infoCount,
      const VkAccelerationStructureBuildGeometryInfoKHR *pInfos,
      const VkAccelerationStructureBuildRangeInfoKHR *const *ppBuildRangeInfos,
      uint32_t readCount, const VkBuffer *bufferReads, uint32_t writeCount,
      const VkBuffer *bufferWrites) -> Error;
  auto
  CopyAccelerationStructureKHR(const VkCopyAccelerationStructureInfoKHR *pInfo)
      -> Error;
  auto ResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery,
                      uint32_t queryCount) -> Error;
  auto WriteAccelerationStructuresPropertiesKHR(
      uint32_t accelerationStructureCount,
      const VkAccelerationStructureKHR *pAccelerationStructures,
      VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery)
      -> Error;
  auto BindIndexBuffer(const Args::VkCmdBindIndexBuffer &arguments) -> void;
  auto BindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount,
                         const VkBuffer *pBuffers, const VkDeviceSize *pOffsets)
      -> void;
  auto SetVertexInputEXT(
      uint32_t vertexBindingDescriptionCount,
      const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
      uint32_t vertexAttributeDescriptionCount,
      const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions)
      -> void;
  auto ClearAttachments(uint32_t attachmentCount,
                        const VkClearAttachment *pAttachments,
                        uint32_t rectCount, const VkClearRect *pRects) -> Error;
  auto
  BeginDebugUtilsLabelEXT(const Args::VkCmdBeginDebugUtilsLabelEXT &arguments)
      -> void;
  auto EndDebugUtilsLabelEXT(const Args::VkCmdEndDebugUtilsLabelEXT &arguments)
      -> void;
  auto
  InsertDebugUtilsLabelEXT(const Args::VkCmdInsertDebugUtilsLabelEXT &arguments)
      -> void;
  auto PipelineBarrier2(const VkDependencyInfo *pDependencyInfo) -> Error;

  [[nodiscard]] auto GetQueueFamily() const -> uint32_t { return queueFamily; }

  auto Reset() -> void;

  auto AddCommand(CommandID command) -> Error;

  std::vector<Command> commandStorage;
  std::vector<uint16_t> commands;

  [[nodiscard]] auto GetCommand(uint32_t identifier) -> Command & {
    return commandStorage.at(identifier);
  }

  CommandID currentRenderPass = InvalidCommandID;

  uint32_t queueFamily;
  VirtualState currentState;
  std::vector<std::vector<uint32_t>> caches;
  VCmdThreadInfo threadInfo;
  bool initialized = false;

  ~VirtualCommandBuffer() { threadInfo.Shutdown(); }

  auto GetNewCommand(CommandType type) -> CommandID;

private:
  auto NewRenderpass() -> void;
};

auto CreateCommandBuffer() -> VirtualCommandBuffer;

} // namespace Graphics