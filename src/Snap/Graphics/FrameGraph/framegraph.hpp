#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#define OUTPUT_DEBUG_GRAPH 0

namespace Graphics {

using CommandLevel = uint16_t;

struct Level {
  CommandLevel level;

  std::vector<CommandIndex> commands;
  std::vector<CommandIndex> userBarriers;

  // Barriers to be executed BEFORE these commands
  std::vector<VkMemoryBarrier2> barriers;
};

struct RenderingInfo {
  VkRenderingInfo info{};
  CommandIndex from = InvalidCommandID;
  CommandIndex to = InvalidCommandID;
};

using CommandIDPair = std::pair<CommandID, uint16_t>;

struct FrameGraph {
  // also defined in command.hpp
  static inline const CommandLevel InvalidDepth = UINT16_MAX;

  auto Submit(const GraphicsContext &context,
              std::vector<VirtualCommandBuffer *> &newCommandBuffers) -> Error;
  auto Write(const GraphicsContext &context, VkCommandBuffer cmdBuffer)
      -> Error;

private:
  std::vector<VirtualCommandBuffer *> commandBuffers;

  // reordered commands
  std::vector<CommandID> commands;

  // linear Command ID -> Command ID in command buffer
  std::vector<CommandIDPair> commandIndices;

  std::vector<Level> graph;
  std::unordered_map<CommandIndex, LoadOpConfig> loadOpConfigs;

  // Direct dependencies per command, after transitive reduction. Used for
  // scheduling (levels, reordering) only: a dropped edge here is redundant
  // for ordering, but its access/stage info would be lost if barrier
  // computation walked this list too. See commandHazardSources.
  std::vector<std::vector<CommandIndex>> commandParents;
  std::vector<std::vector<CommandIndex>> nextReady;

  // Quick early-out to see if resources were edited.
  std::unordered_set<void *> resourcesWritesInFrame;

  // Every command whose resource access this command must synchronize
  // against, before transitive reduction. Unlike commandParents, no entry
  // is ever dropped for being reachable through another: two commands can
  // both be real hazard sources (e.g. a write W and a later read R of W,
  // where a subsequent write needs W's access info even though R already
  // orders after W). GetRequiredBarriers walks this list so no writer's
  // access/stage flags are silently lost.
  std::vector<std::vector<CommandIndex>> commandHazardSources;

  // Scratch data for the reachability walk in ReduceParents.
  std::vector<uint32_t> ancestorStamps;
  std::vector<CommandIndex> ancestorStack;

#if OUTPUT_DEBUG_GRAPH
  // Temporary data for Graphviz
  std::unordered_set<uint32_t> dependencies; // a | b sorted asc

  auto DebugOutput() -> Error;
#endif

  auto MapResourceUsages() -> Error;
  auto PreCompile() -> Error;
  auto Compile(const GraphicsContext &context) -> Error;
  auto BuildGraph() -> Error;
  auto MarkAncestors(CommandIndex from, CommandIndex floor, uint32_t stamp)
      -> void;
  auto ReduceParents(CommandIndex idx,
                     const std::vector<CommandIndex> &candidates) -> void;
  auto CompactRenderPasses() -> Error;
  auto BuildRenderRegions(const GraphicsContext &context)
      -> Result<std::vector<RenderingInfo>>;

  auto InsertBarriers() -> Error;
  auto BuildReadyState() -> Error;

  auto GetRequiredBarriers(CommandID commandId, const VulkanResource &resource,
                           VkAccessFlags2 accesses,
                           VkPipelineStageFlags2 pipelines,
                           std::vector<VkMemoryBarrier2> &memoryBarriers)
      -> void;
  auto SyncMask(CommandLevel start, CommandLevel end,
                VkAccessFlags2 lastWriteAccess,
                VkPipelineStageFlags2 lastWritePipeline,
                VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstPipeline)
      -> std::array<VkPipelineStageFlags2, UINT64_WIDTH>;
  auto PickNextCommands(CommandIndex parent) -> std::vector<CommandIndex>;
  auto ScoreCommand(CommandIndex parent, CommandIndex child) -> uint32_t;
  auto Reorder() -> Result<std::vector<CommandIndex> *>;
  auto UpdateLevels(const std::vector<CommandIndex> &reordered) -> Error;
  auto GetCommandLevel(CommandIndex commandId) -> CommandLevel;
  auto Reset() -> void;
  auto BuildLoadOpModes() -> void;
  auto GetCommand(CommandIDPair identifier) -> Command &;
  auto GetCommand(CommandID identifier) -> Command &;
  static auto WriteCommand(const Command &command,
                           const GraphicsContext &context,
                           VkCommandBuffer cmdBuffer,
                           const LoadOpConfig *loadOpConfig) -> Error;
};
} // namespace Graphics