#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_core.h>

#define OUTPUT_DEBUG_GRAPH 0

namespace Graphics {

using CommandLevel = uint16_t;

struct Level {
  CommandLevel level;

  std::vector<CommandID> commands;
  std::vector<CommandID> userBarriers;

  // Barriers to be executed BEFORE these commands
  std::vector<VkMemoryBarrier2> barriers;
};

struct RenderingInfo {
  VkRenderingInfo info{};
  CommandID from = InvalidCommandID;
  CommandID to = InvalidCommandID;
};

struct FrameGraph {
  // also defined in command.hpp
  static inline const CommandLevel InvalidDepth = UINT16_MAX;

  auto Submit(const GraphicsContext &context,
              const VirtualCommandBuffer &commands) -> Error;
  auto Write(const GraphicsContext &context, VkCommandBuffer cmdBuffer)
      -> Error;

private:
  VirtualCommandBuffer commandBuffer;

  std::vector<Level> graph;
  std::unordered_map<CommandID, LoadOpConfig> loadOpConfigs;

  // Direct dependencies per command, after transitive reduction. Used for
  // scheduling (levels, reordering) only: a dropped edge here is redundant
  // for ordering, but its access/stage info would be lost if barrier
  // computation walked this list too. See commandHazardSources.
  std::vector<std::vector<CommandID>> commandParents;
  std::vector<std::vector<CommandID>> nextReady;

  // Every command whose resource access this command must synchronize
  // against, before transitive reduction. Unlike commandParents, no entry
  // is ever dropped for being reachable through another: two commands can
  // both be real hazard sources (e.g. a write W and a later read R of W,
  // where a subsequent write needs W's access info even though R already
  // orders after W). GetRequiredBarriers walks this list so no writer's
  // access/stage flags are silently lost.
  std::vector<std::vector<CommandID>> commandHazardSources;

  // Scratch data for the reachability walk in ReduceParents.
  std::vector<uint32_t> ancestorStamps;
  std::vector<CommandID> ancestorStack;

#if OUTPUT_DEBUG_GRAPH
  // Temporary data for Graphviz
  std::unordered_set<uint32_t> dependencies; // a | b sorted asc

  auto DebugOutput() -> Error;
#endif

  auto MapResourceUsages() -> Error;
  auto PreCompile() -> Error;
  auto Compile(const GraphicsContext &context) -> Error;
  auto BuildGraph() -> Error;
  auto MarkAncestors(CommandID from, CommandID floor, uint32_t stamp) -> void;
  auto ReduceParents(CommandID idx, const std::vector<CommandID> &candidates)
      -> void;
  auto BuildRenderRegions(const GraphicsContext &context)
      -> Result<std::vector<RenderingInfo>>;

  auto ResourceAccessAt(CommandID commandId, const VulkanResource &resource)
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;
  auto ResourceReadsAt(CommandID commandId, const VulkanResource &resource)
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;
  auto ResourceWritesAt(CommandID commandId, const VulkanResource &resource)
      -> std::pair<VkAccessFlags2, VkPipelineStageFlags2>;

  auto ValidateGraph() -> Error;
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
  auto PickNextCommands(CommandID parent) -> std::vector<CommandID>;
  auto ScoreCommand(CommandID parent, CommandID child) -> uint32_t;
  auto Reorder() -> Result<std::vector<CommandID>>;
  auto UpdateLevels(const std::vector<CommandID> &reordered) -> Error;
  auto GetCommandLevel(CommandID commandId) -> CommandLevel;
  auto Reset() -> void;
  auto BuildLoadOpModes() -> void;
};
} // namespace Graphics