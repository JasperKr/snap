#include "framegraph.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/dynamicRendering.hpp"
#include "Graphics/FrameGraph/pipelineState.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/descriptorHeap.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/vkAccessHelpers.hpp"
#include "Graphics/vkPipelineHelpers.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_set>
#include <variant>
#if OUTPUT_DEBUG_GRAPH
#include "Modules/filesystem.hpp"
#include <format>
#endif
#include "Modules/timer.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto FrameGraph::Submit(const GraphicsContext &context,
                        std::vector<VirtualCommandBuffer *> &newCommandBuffers)
    -> Error {
  commandBuffers.clear();
  commandIndices.clear();
  uint16_t idx = 1UL;

  for (auto &commandBuffer : newCommandBuffers) {
    commandBuffers.emplace_back(commandBuffer);
    for (CommandID identifier : commandBuffer->commands) {
      commandIndices.emplace_back(identifier, idx);
    }

    idx++;
  }

  CHECK_ERR(Compile(context));

  return {};
}

auto FrameGraph::GetCommand(CommandIDPair identifier) -> Command & {
  auto idx = identifier.second;
  assert(idx != 0);
  return commandBuffers.at(idx - 1)->commandStorage.at(identifier.first);
}

auto FrameGraph::GetCommand(CommandID identifier) -> Command & {
  return GetCommand(commandIndices.at(identifier));
}

auto FrameGraph::MapResourceUsages() -> Error {
  ZoneScoped;

  {
    ZoneScopedN("Map writes");

    for (const auto command : commandIndices) {
      const auto *boundState =
          get_if_derived<BoundResources>(GetCommand(command).data);

      if (boundState == nullptr) {
        continue;
      }

      for (const auto &write : boundState->writes) {
        resourcesWritesInFrame.emplace(write.Ptr());
      }
    }
  }

  {
    ZoneScopedN("Erase irrelevant data");
    Utils::ParallelFor(commandIndices, [this](CommandIDPair command) -> void {
      auto *boundState =
          get_if_derived<BoundResources>(GetCommand(command).data);

      if (boundState == nullptr) {
        return;
      }

      Utils::UnorderedErase(
          boundState->reads, [this](const VulkanResource &resource) -> bool {
            return !resourcesWritesInFrame.contains(resource.Ptr());
          });
    });
  }

  return {};
}

// Marks every strict ancestor of `from` with `stamp`, walking the already
// reduced parent edges. Commands older than `floor` are never expanded: any
// path between two candidate parents only passes through commands younger than
// the oldest candidate, so cutting there does not change the outcome.
auto FrameGraph::MarkAncestors(CommandIndex from, CommandIndex floor,
                               uint32_t stamp) -> void {
  ancestorStack.clear();
  ancestorStack.emplace_back(from);

  while (!ancestorStack.empty()) {
    auto node = ancestorStack.back();
    ancestorStack.pop_back();

    for (const auto parent : commandParents[node]) {
      if (parent < floor || ancestorStamps[parent] == stamp) {
        continue;
      }

      ancestorStamps[parent] = stamp;
      ancestorStack.emplace_back(parent);
    }
  }
}

// Reduces `candidates` (sorted descending, no duplicates) to the maximal
// commands under reachability and stores them as the parents of `idx`.
// A candidate is dropped when another candidate already depends on it, so
//   A -> B -> C
//   D ------> C
// keeps both B -> C and D -> C, but drops A -> C.
auto FrameGraph::ReduceParents(CommandIndex idx,
                               const std::vector<CommandIndex> &candidates)
    -> void {
  auto &parents = commandParents[idx];
  parents.clear();

  if (candidates.empty()) {
    return;
  }

  // Ancestors are always older, so walking newest first guarantees that a
  // candidate reachable from another candidate is already stamped by the time
  // we get to it.
  const auto floor = candidates.back();

  for (const auto candidate : candidates) {
    if (ancestorStamps[candidate] == idx) {
      continue; // Already reachable through a candidate we kept.
    }

    parents.emplace_back(candidate);
    MarkAncestors(candidate, floor, idx);
  }
}

// NOLINTNEXTLINE
auto FrameGraph::PreCompile() -> Error {
  ZoneScoped;

  BuildLoadOpModes();

  const auto commandCount = commands.size();

  commandParents.assign(commandCount, {});
  commandHazardSources.assign(commandCount, {});
  ancestorStamps.assign(commandCount, UINT32_MAX);
  ancestorStack.reserve(commandCount);

  // Per resource frontier: the last writer plus every read since that write.
  // Older usages are never candidates, they are reached through the frontier
  // itself (writes chain through WAW, reads before the last write are already
  // WAR parents of it).
  struct Frontier {
    CommandIndex lastWrite = InvalidCommandID;
    std::vector<CommandIndex> readsSinceWrite;
  };

  // Resources only ever overlap when they share the same underlying handle
  // (same VkImage, VkBuffer, or acceleration structure) -- different handles
  // never alias. So bucket frontiers by handle: overlap/equality scans then
  // only cover the sub-ranges of one resource (a handful of mips/layers at
  // most) instead of every resource touched anywhere in the command buffer.

  struct FrontierKey {
    VulkanResource key;

    FrontierKey(const VulkanResource &key) : key(key) {} // NOLINT

    auto operator==(const FrontierKey &other) const noexcept -> bool {
      return key.Overlaps(other.key);
    }
  };

  struct FrontierKeyHash {
    auto operator()(const FrontierKey &item) const -> uint64_t {
      return reinterpret_cast<uint64_t>(item.key.Ptr()); // NOLINT
    }
  };

  std::unordered_map<FrontierKey,
                     std::vector<std::pair<VulkanResource, Frontier>>,
                     FrontierKeyHash>
      frontiers;
  static std::vector<CommandIndex> candidates;

  for (const CommandID commandID : commands) {
    auto &command = GetCommand(commandID);
    ZoneScopedN("Determine command parents");

    candidates.clear();

    for (const auto &resource : GetReads(command)) { // RAW
      const auto bucketIt = frontiers.find(resource);
      if (bucketIt == frontiers.end()) {
        continue;
      }

      for (auto &[existing, frontier] : bucketIt->second) {
        if (!existing.Overlaps(resource)) {
          continue;
        }

        if (frontier.lastWrite != InvalidCommandID) {
          candidates.emplace_back(frontier.lastWrite);
        }
      }
    }

    for (const auto &resource : GetWrites(command)) {
      const auto bucketIt = frontiers.find(resource);
      if (bucketIt == frontiers.end()) {
        continue;
      }

      for (auto &[existing, frontier] : bucketIt->second) {
        if (!existing.Overlaps(resource)) {
          continue;
        }

        if (frontier.lastWrite != InvalidCommandID) { // WAW
          candidates.emplace_back(frontier.lastWrite);
        }

        candidates.append_range(frontier.readsSinceWrite); // WAR
      }
    }

    Utils::DeDuplicate(candidates);

    CommandLevel level = 0;
    for (const auto parentID : candidates) {
      const auto &parent = GetCommand(commands.at(parentID));

      const auto parentLevel = parent.level;
      ERR_ASSERT(parentLevel != InvalidDepth);

      level = std::max<CommandLevel>(level, parentLevel + 1);
    }

    command.level = level;
    commandHazardSources[command.id] = candidates;

    ReduceParents(command.id, candidates);

    for (const auto &resource : GetWrites(command)) {
      auto &bucket = frontiers[resource];

      bool found = false;
      for (auto &[existing, frontier] : bucket) {
        if (existing != resource) {
          continue;
        }

        frontier.lastWrite = command.id;
        frontier.readsSinceWrite.clear();
        found = true;
        break;
      }
      if (!found) {
        bucket.emplace_back(
            resource, Frontier{.lastWrite = command.id, .readsSinceWrite = {}});
      }
    }

    for (const auto &resource : GetReads(command)) {
      auto &bucket = frontiers[resource];

      bool found = false;
      for (auto &[existing, frontier] : bucket) {
        if (existing != resource) {
          continue;
        }

        frontier.readsSinceWrite.emplace_back(command.id);
        found = true;
        break;
      }

      if (!found) {
        bucket.emplace_back(resource,
                            Frontier{.lastWrite = InvalidCommandID,
                                     .readsSinceWrite = {command.id}});
      }
    }
  }

  candidates.clear();

  return {};
}

auto FrameGraph::BuildGraph() -> Error {
  ZoneScoped;

  auto time = Timer::GetTime();
  CHECK_ERR(PreCompile());
  auto time2 = Timer::GetTime();

  CommandLevel maxLevel{};

  for (const CommandID commandID : commands) {
    const auto &command = GetCommand(commandID);
    maxLevel = std::max(maxLevel, command.level);
  }

  graph.resize(maxLevel + 1);
  for (auto i = 0; i < graph.size(); i++) {
    graph.at(i).level = i;
  }

  size_t edgeCount = 0;
  for (const CommandID commandID : commands) {
    const auto &command = GetCommand(commandID);

    ERR_ASSERT(command.level != InvalidDepth);
    ERR_ASSERT(command.level < maxLevel + 1);

    graph[command.level].commands.emplace_back(command.id);

#if OUTPUT_DEBUG_GRAPH
    for (const CommandID parentId : commandParents[command.id]) {
      const auto &parent = commands.at(parentId);

      if (parent.GetType() != CommandType::vkCmdPipelineBarrier2 &&
          command.GetType() != CommandType::vkCmdPipelineBarrier2) {
        dependencies.emplace((uint32_t(parentId) << UINT16_WIDTH) | command.id);
      }
    }
#endif

    edgeCount += commandParents[command.id].size();
  }
  auto time3 = Timer::GetTime();

  // PrintAlways("Build time: {}", time3 - time);
  // PrintAlways(" - Precompile: {}", time2 - time);
  // PrintAlways(" - Level: {}", time3 - time2);
  // PrintAlways("Dependency edges: {}", edgeCount);

  return {};
}

auto MemoryBarrierToString(const VkMemoryBarrier2 &barrier) -> std::string {
  std::stringstream stream{};

  stream << VkAccessHelpers::AccessFlags2ToString(barrier.srcAccessMask)
         << " at "
         << VkPipelineHelpers::PipelineStage2ToString(barrier.srcStageMask)
         << " -> "
         << VkAccessHelpers::AccessFlags2ToString(barrier.dstAccessMask)
         << " at "
         << VkPipelineHelpers::PipelineStage2ToString(barrier.dstStageMask);

  return stream.str();
}

#if OUTPUT_DEBUG_GRAPH
// NOLINTNEXTLINE
auto FrameGraph::DebugOutput() -> Error {
  std::stringstream stream;
  stream << "digraph FrameGraph {\n";
  stream << "rankdir=LR;\n";
  stream << R"(
    bgcolor="#1e1e1e";

    node [
        color="#888888",
        fontcolor="white",
        style="filled",
        fillcolor="#2d2d2d"
    ];

    edge [
        color="#aaaaaa",
        fontcolor="white"
    ];
  )";

  const static bool ExportRWLabels = true;

  for (const auto &level : graph) {
    stream << std::format("subgraph cluster_level_{} {{\n", level->level);
    stream << std::format("label = \"Level {}\";\n", level->level);

    for (const auto commandID : level.commands) {
      const auto &command = commands.at(commandID);
      if (ExportRWLabels) {
        const auto &reads = GetReads(command);
        const auto &writes = GetWrites(command);

        std::stringstream readStr;
        std::stringstream writeStr;

        for (const auto &resource : reads) {
          readStr << resource.ToString();

          if (resource != reads.back()) {
            readStr << "\n";
          }
        }

        for (const auto &resource : writes) {
          writeStr << resource.ToString();

          if (resource != writes.back()) {
            writeStr << "\n";
          }
        }

        stream << std::format(
            "{} [label=\"{}: {}\nR: {}\nW: {}\"];\n", command->id,
            command->level, CommandTypeEnumHelper.ToString(command->GetType()),
            readStr.str(), writeStr.str());
      } else {
        stream << std::format(
            "{} [label=\"{}: {}\"];\n", command->id, command->level,
            CommandTypeEnumHelper.ToString(command->GetType()));
      }
    }

    stream << std::format("barriers_{} [\n", level->level);

    std::stringstream barriers{};

    for (const auto &barrier : level.barriers) {
      barriers << MemoryBarrierToString(barrier) << "\n";
    }

    stream << std::format("shape=note,\nlabel=\"{} Barriers\"\n",
                          barriers.str());
    stream << "];\n}\n";
  }

  for (const auto dependency : dependencies) {
    uint32_t parent = (dependency >> UINT16_WIDTH) & UINT16_MAX;
    uint32_t child = dependency & UINT16_MAX;

    ERR_ASSERT_MSG(
        commands.at(child)->level > commands.at(parent)->level,
        std::format(
            "child: {} with parent: {}, child level {} <= parent level {}",
            child, parent, commands.at(child)->level,
            commands.at(parent)->level));

    stream << "\"" << parent << "\" -> \"" << child << "\";\n";
  }

  stream << "}\n";

  CHECK_ERR(Filesystem::WriteFile("framegraph.dot", stream.str()));
  PrintAlways("Finalized writing .dot file");

  return {};
}
#endif

// This is probably quite broken in multiple ways
// But the idea is there

// Non inclusive start -> inclusive end
// Why? Commands are ran after barriers on a level, so if we query synced mask 5 -> 11
// Commands at 5 may cause a hazard, but 11 not if we added a barrier there beforehand.
// The barrier mask can be used with bitwise and to check if some bit is not yet synced.
// Returns the unsynced access bits as a 64x64 matrix, as access rows pipeline column
auto FrameGraph::SyncMask(CommandLevel start, CommandLevel end,
                          VkAccessFlags2 lastWriteAccess,
                          VkPipelineStageFlags2 lastWritePipeline,
                          VkAccessFlags2 dstAccess,
                          VkPipelineStageFlags2 dstPipeline)
    -> std::array<VkPipelineStageFlags2, UINT64_WIDTH> {

  // It is illegal to use frame graphs in other threads, and so we can keep this static
  static std::array<VkPipelineStageFlags2, UINT64_WIDTH> maskBits{};
  memset(maskBits.data(), 0, (UINT64_WIDTH * sizeof(VkPipelineStageFlags2)));

  // all bits that we access.
  // the for loop than iterates over all barriers
  // checks if the destination stages match
  // and if the source pipeline and access match too
  // if so, mark those bits as synced.

  // Fast way to iterate over set bits
  for (auto bitIndex : Utils::BitIndexRange(dstPipeline)) {
    maskBits.at(bitIndex) = dstAccess;
  }

  // mask bits are now all unsynced bits
  for (CommandLevel level = end; level >= start + 1U; level--) {
    const auto &barriers = graph.at(level).barriers;

    for (const auto &barrier : barriers) {
      if (dstPipeline == 0ULL) {
        return maskBits;
      }

      const VkPipelineStageFlags2 sharedPipelines =
          barrier.dstStageMask & dstPipeline;

      for (const uint32_t bitIndex : Utils::BitIndexRange(sharedPipelines)) {
        // If this stage bit is active in this sync and we still need it
        const uint64_t mask = 1ULL << bitIndex;

        const bool srcMatch =
            // Match the sync's source stage with our current bit
            (barrier.srcStageMask & lastWritePipeline) != 0ULL &&
            // Match the sync's source access with our resource's last usage
            (barrier.srcAccessMask & lastWriteAccess) != 0ULL;

        if (srcMatch) {
          // Remove the accesses that are now satisfied
          maskBits.at(bitIndex) &= ~barrier.dstAccessMask;

          // All synced
          if (maskBits.at(bitIndex) == 0ULL) {
            // Remove this pipeline to be checked
            dstPipeline &= ~mask;
          }
        }
      }
    }
  }

  return maskBits;
}

inline auto ResourceListToString(const std::vector<void *> &resources)
    -> std::string {
  std::stringstream str;

  for (const auto *resource : resources) {
    str << resource;
    if (resource != resources.back()) {
      str << "; ";
    }
  }

  return str.str();
}

auto FrameGraph::GetRequiredBarriers(
    CommandID commandID, const VulkanResource &resource,
    VkAccessFlags2 accesses, VkPipelineStageFlags2 pipelines,
    std::vector<VkMemoryBarrier2> &memoryBarriers) -> void {
  const auto &command = GetCommand(commandID);

  if (command.level == 0) {
    return;
  }

  // Walks every real hazard source for this resource, not just the
  // transitively-reduced commandParents: a source dropped there for being
  // reachable through another kept candidate can still be the only command
  // carrying the write access/stage info this barrier needs.
  const auto &hazardSources = commandHazardSources[command.id];

  for (const auto &parentID : hazardSources) {
    const auto &parent = GetCommand(commands.at(parentID));
    const auto *bound = get_if_derived<BoundResources>(parent.data);

    if (bound == nullptr) {
      continue;
    }

    auto iter = bound->accesses.find(resource);

    if (iter == bound->accesses.end()) {
      continue;
    }

    const auto [srcAccess, srcStage] = iter->second;
    // This is a VALID result. In the scenario, for example, read x, write y, and our parent writes only x / y, we will
    // loop over the parent for both x, y, and notice in one scenario that either x or y is not viewed by the parent and thus
    // it has no access or stage.
    if (srcAccess == 0U || srcStage == 0U) {
      continue;
    }

    // SyncMask returns all accesses per pipeline not yet synced
    // For the pipelines we ask about
    const auto &mask = SyncMask(parent.level, command.level, srcAccess,
                                srcStage, accesses, pipelines);

    for (const auto bit : Utils::BitIndexRange(pipelines)) {
      const VkPipelineStageFlags2 unsyncedAccesses = mask.at(bit);

      if (unsyncedAccesses == 0ULL ||
          !VkAccessHelpers::IsWriteAccess(unsyncedAccesses | srcAccess)) {
        continue;
      }

      VkMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;

      // Not yet synced previously, we need to take in to account
      // all bits like write A, B, C but read only B, C, we still need to sync for A,
      // so we do not bitop away A.
      barrier.srcAccessMask = srcAccess;

      // Pipeline stage for which these accesses weren't synced
      barrier.srcStageMask = srcStage;

      barrier.dstAccessMask = unsyncedAccesses;
      barrier.dstStageMask = 1ULL << bit;

      memoryBarriers.emplace_back(barrier);
    }
  }
}

inline auto MergeBarriers(std::vector<VkMemoryBarrier2> &barriers) {
  static std::vector<VkMemoryBarrier2> newBarriers;
  newBarriers.clear();

  for (const auto &barrier : barriers) {
    bool combined = false;
    for (auto &other : newBarriers) {
      if (other.srcStageMask == barrier.srcStageMask &&
          other.dstStageMask == barrier.dstStageMask) {
        other.srcAccessMask |= barrier.srcAccessMask;
        other.dstAccessMask |= barrier.dstAccessMask;

        combined = true;
        break;
      }
    }

    if (!combined) {
      newBarriers.emplace_back(barrier);
    }
  }

  barriers = newBarriers;
}

auto FrameGraph::InsertBarriers() -> Error {
  ZoneScoped;

  {
    ZoneScopedN("Filter user barriers");
    for (auto &level : graph) {
      Utils::UnorderedErase(
          level.commands, [&](const CommandIndex commandIndex) -> bool {
            const CommandID commandID = commands.at(commandIndex);
            const auto &command = GetCommand(commandID);

            if (command.GetType() == CommandType::vkCmdPipelineBarrier2) {
              level.userBarriers.emplace_back(commandID);

              return true;
            }
            return false;
          });
    }
  }

  const auto addBarriers = [this](const std::vector<VulkanResource> &resources,
                                  const CommandID commandID,
                                  Graphics::Level &level) -> void {
    for (const auto &resource : resources) {
      if (!resourcesWritesInFrame.contains(resource.Ptr())) {
        continue;
      }

      const auto &command = GetCommand(commandID);
      const auto *bound = get_if_derived<BoundResources>(command.data);

      if (bound == nullptr) {
        continue;
      }

      auto iter = bound->accesses.find(resource);

      if (iter == bound->accesses.end()) {
        continue;
      }

      const auto [accesses, pipelines] = iter->second;

      GetRequiredBarriers(commandID, resource, accesses, pipelines,
                          level.barriers);
    }
  };

  for (auto &level : graph) {
    ZoneScopedN("level barriers");
    for (const auto levelCmdID : level.commands) {
      const auto &commandID = commands.at(levelCmdID);
      const auto &command = GetCommand(commandID);

      // if (command->GetType() == CommandType::renderPass) {

      // } else {
      addBarriers(GetReads(command), commandID, level);
      addBarriers(GetWrites(command), commandID, level);
      // }
    }

    MergeBarriers(level.barriers);
  }

  return {};
}

inline auto GetRenderExtent(const Graphics::GraphState &state) -> VkExtent2D {
  assert(state.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS);

  if (!state.colorAttachments.empty()) {
    return VkExtent2D{
        .width = state.colorAttachments.front().texture->GetWidth(),
        .height = state.colorAttachments.front().texture->GetHeight(),
    };
  }

  if (state.hasDepthStencilAttachment) {
    return VkExtent2D{
        .width = state.depthStencilAttachment.texture->GetWidth(),
        .height = state.depthStencilAttachment.texture->GetHeight(),
    };
  }

  assert(false && "Trying to get render extent with no attachments.");
  return VkExtent2D{0, 0};
}

inline auto DrawStateToRenderingInfo(const GraphicsContext &context,
                                     const GraphState &renderState)
    -> Result<VkRenderingInfo> {
  ZoneScoped;

  VkRenderingInfo renderingInfo = {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

  renderingInfo.renderArea.offset = {.x = 0, .y = 0};
  renderingInfo.renderArea.extent = GetRenderExtent(renderState);
  renderingInfo.layerCount = 1;
  renderingInfo.viewMask = 0;
  renderingInfo.flags = 0;

  if (renderState.colorAttachments.size() >
      context.deviceProperties.limits.maxColorAttachments) {
    return Error::Create(
        "Number of bound render targets exceeds device limits.");
  }

  if (renderingInfo.renderArea.extent.width == 0 ||
      renderingInfo.renderArea.extent.height == 0) {
    return Error::Create("Render area has zero width or height.");
  }

  auto colorAttachments =
      std::array<VkRenderingAttachmentInfo, MAX_COLOR_ATTACHMENTS>{};
  auto depthAttachment = VkRenderingAttachmentInfo{};
  auto stencilAttachment = VkRenderingAttachmentInfo{};

  for (int i = 0; i < renderState.colorAttachments.size(); i++) {
    const auto &rendertarget = renderState.colorAttachments.at(i);
    // CHECK_ERR(
    //     rendertarget.texture->UseAsAttachment(context, rendertarget.loadOp));

    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachmentInfo.loadOp = rendertarget.loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    colorAttachments.at(i) = attachmentInfo;
  }

  bool hasDepth = false;
  bool hasStencil = false;

  if (renderState.hasDepthStencilAttachment) {
    const auto &rendertarget = renderState.depthStencilAttachment;
    VkRenderingAttachmentInfo attachmentInfo = {};
    attachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    attachmentInfo.imageView = rendertarget.texture->view;
    attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachmentInfo.loadOp = rendertarget.loadOp;
    attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentInfo.clearValue = rendertarget.clearValue;

    if (rendertarget.texture->IsDepthTexture()) {
      depthAttachment = attachmentInfo;
      hasDepth = true;
    }

    if (rendertarget.texture->IsStencilTexture()) {
      stencilAttachment = attachmentInfo;
      hasStencil = true;
    }
  }

  renderingInfo.colorAttachmentCount = renderState.colorAttachments.size();
  renderingInfo.pColorAttachments = colorAttachments.data();

  renderingInfo.pStencilAttachment = hasStencil ? &stencilAttachment : nullptr;
  renderingInfo.pDepthAttachment = hasDepth ? &depthAttachment : nullptr;

  // Make sure subsequent renders load from the existing content if we ever need to re-bind mid-pass
  // for (auto &rendertarget : renderState.colorAttachments) {
  //   rendertarget.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  // }

  // if (renderState.hasDepthStencilAttachment) {
  //   renderState.depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  // }

  return renderingInfo;
}

inline auto CompareRenderingInfos(const VkRenderingInfo &first,
                                  const VkRenderingInfo &second) -> bool {
  if (first.renderArea.offset.x != second.renderArea.offset.x ||
      first.renderArea.offset.y != second.renderArea.offset.y ||
      first.renderArea.extent.width != second.renderArea.extent.width ||
      first.renderArea.extent.height != second.renderArea.extent.height) {
    return false;
  }

  if (first.colorAttachmentCount != second.colorAttachmentCount) {
    return false;
  }

  std::span<const VkRenderingAttachmentInfo> firstColors(
      first.pColorAttachments, first.colorAttachmentCount);
  std::span<const VkRenderingAttachmentInfo> secondColors(
      second.pColorAttachments, second.colorAttachmentCount);

  for (size_t i = 0; i < firstColors.size(); ++i) {
    const auto &firstAttach = firstColors[i];
    const auto &secondAttach = secondColors[i];
    if (firstAttach.imageView != secondAttach.imageView ||
        firstAttach.imageLayout != secondAttach.imageLayout ||
        firstAttach.loadOp != secondAttach.loadOp ||
        firstAttach.storeOp != secondAttach.storeOp) {
      return false;
    }
  }

  const bool firstHasDepth = first.pDepthAttachment != nullptr;
  const bool secondHasDepth = second.pDepthAttachment != nullptr;
  if (firstHasDepth != secondHasDepth) {
    return false;
  }
  if (firstHasDepth && secondHasDepth) {
    if (first.pDepthAttachment->imageView !=
            second.pDepthAttachment->imageView ||
        first.pDepthAttachment->imageLayout !=
            second.pDepthAttachment->imageLayout ||
        first.pDepthAttachment->loadOp != second.pDepthAttachment->loadOp ||
        first.pDepthAttachment->storeOp != second.pDepthAttachment->storeOp) {
      return false;
    }
  }

  const bool firstHasStencil = first.pStencilAttachment != nullptr;
  const bool secondHasStencil = second.pStencilAttachment != nullptr;
  if (firstHasStencil != secondHasStencil) {
    return false;
  }
  if (firstHasStencil && secondHasStencil) {
    if (first.pStencilAttachment->imageView !=
            second.pStencilAttachment->imageView ||
        first.pStencilAttachment->imageLayout !=
            second.pStencilAttachment->imageLayout ||
        first.pStencilAttachment->loadOp != second.pStencilAttachment->loadOp ||
        first.pStencilAttachment->storeOp !=
            second.pStencilAttachment->storeOp) {
      return false;
    }
  }

  return true;
}

auto FrameGraph::BuildRenderRegions(const GraphicsContext &context)
    -> Result<std::vector<RenderingInfo>> {
  ZoneScoped;

  std::vector<RenderingInfo> infos{};
  size_t currentIndex = SIZE_MAX;

  for (const auto &level : graph) {
    for (const CommandID commandID : level.commands) {
      const auto &command = GetCommand(commandID);

      const auto *drawState = command.GetDrawState();
      const auto *callableState = get_if_derived<Callable>(command.data);
      const auto type = command.GetType();

      // Any non-draw command breaks the current contiguous rendering region.
      if ((drawState == nullptr || callableState == nullptr ||
           !GetRequiresRendering(type)) &&
          type != CommandType::renderPass) {
        currentIndex = SIZE_MAX;
        continue;
      }

      uint32_t stateID{};

      if (type == CommandType::renderPass) {
        stateID = std::get<RenderPass>(command.data).stateID;
      } else {
        stateID = drawState->stateID;
      }

      const auto &renderState = CommandStateManager::States.at(stateID);

      if (renderState.bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
        currentIndex = SIZE_MAX;
        continue;
      }

      const auto &renderingInfo =
          CHECK_RES(DrawStateToRenderingInfo(context, renderState));

      if (currentIndex == SIZE_MAX ||
          !CompareRenderingInfos(renderingInfo, infos[currentIndex].info)) {
        infos.emplace_back(renderingInfo);
        currentIndex = infos.size() - 1;
        infos[currentIndex].from = commandID;
      }

      infos[currentIndex].to = commandID;
    }
  }

  return infos;
}

auto FrameGraph::BuildReadyState() -> Error {
  ZoneScoped;

  const auto commandCount = commands.size();

  std::vector<bool> scheduled(commandCount, false);
  size_t scheduledCount = 0;

  nextReady.clear();
  nextReady.resize(commandCount);
  std::vector<CommandID> batch;

  while (scheduledCount < commandCount) {
    batch.clear();

    for (const auto commandID : commands) {
      const auto &command = GetCommand(commandID);
      if (scheduled[command.id]) {
        continue;
      }

      const bool parentsReady = std::all_of(
          commandParents[command.id].begin(), commandParents[command.id].end(),
          [&](CommandID parent) -> auto { return scheduled[parent]; });

      if (parentsReady) {
        batch.emplace_back(command.id);
      }
    }

    if (batch.empty()) {
      return Error::Create("Cycle detected while building ready state");
    }

    for (const CommandID commandID : batch) {
      scheduled[commandID] = true;

      // Find commands that become ready because of commandID.
      for (const auto childID : commands) {
        const auto &command = GetCommand(childID);

        // This command has been scheduled
        if (scheduled[command.id]) {
          continue;
        }

        const auto &parentsOfChild = commandParents[command.id];
        auto iter = std::ranges::find(parentsOfChild, commandID);

        // We are not a parent of this command
        if (iter == parentsOfChild.end()) {
          continue;
        }

        // All parents have been scheduled
        const bool parentsReady =
            std::ranges::all_of(parentsOfChild, [&](CommandID parent) -> auto {
              return scheduled[parent];
            });

        if (parentsReady) {
          nextReady[commandID].emplace_back(command.id);
        }
      }
    }

    scheduledCount += batch.size();
  }

  return {};
}

inline auto ComparePipelineCompatability(const GraphState &state,
                                         const GraphState &other) {
  if (state.colorAttachments != other.colorAttachments) {
    return false;
  }

  if (state.shader != other.shader) {
    return false;
  }

  if (state.hasDepthStencilAttachment != other.hasDepthStencilAttachment) {
    return false;
  }

  if (state.hasDepthStencilAttachment) {
    if (state.depthStencilAttachment != other.depthStencilAttachment) {
      return false;
    }
  }

  return true;
}

auto FrameGraph::ScoreCommand(CommandIndex parentID, CommandIndex childID)
    -> uint32_t {
  const auto &parent = GetCommand(commands.at(parentID));
  const auto &child = GetCommand(commands.at(childID));

  const auto DynamicStateUpdateCost = 100U;
  const auto EndRenderingCost = 500U;
  const auto StartRenderingCost = 1000U;

  uint32_t cost = 0U;

  const auto *parentDrawState = parent.GetDrawState();
  const auto *childDrawState = child.GetDrawState();

  if (parentDrawState != nullptr) { // Parent is rendering
    const auto *graphState = parentDrawState->GetGraphState();

    if (childDrawState != nullptr) { // Child is rendering
      const auto *childGraphState = childDrawState->GetGraphState();

      if (!ComparePipelineCompatability(*graphState, *childGraphState)) {
        cost += EndRenderingCost;

        if (childGraphState->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
          cost += StartRenderingCost;
        }

        // Full compat check, including blend mode, back face etc..
      } else if (graphState != childGraphState) {
        cost += DynamicStateUpdateCost;
      }
    } else { // Child is not rendering
      cost += EndRenderingCost;
    }
  } else if (childDrawState != nullptr) { // Child is rendering but parent isn't
    const auto &childGraphState = childDrawState->GetGraphState();
    if (childGraphState->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      cost += StartRenderingCost;
    }
  } else { // Neither child nor parent are rendering
    // no extra cost to call
  }

  return cost;
}

// returns a list of commands, with the best pick last
auto FrameGraph::PickNextCommands(CommandIndex parent)
    -> std::vector<CommandIndex> {
  ZoneScoped;

  const auto &children = nextReady.at(parent);

  [[unlikely]]
  if (children.empty()) {
    return {};
  }

  if (children.size() == 1) {
    return children;
  }

  uint32_t lowestScore = ScoreCommand(parent, children.front());
  CommandIndex bestCommand = children.front();

  static std::unordered_map<CommandIndex, uint32_t> scores;
  snap_defer(scores.clear());
  scores.reserve(children.size());

  for (CommandIndex child : children) {
    scores.emplace(child, ScoreCommand(parent, child));
  }

  std::vector<CommandIndex> sortedChildren{children};

  std::ranges::sort(
      sortedChildren,
      [](const CommandIndex &first, const CommandIndex &second) -> bool {
        // A(3) > B(2) -> A, B. We want highest score first, so A > B
        return scores.at(first) > scores.at(second);
      });

  return sortedChildren;
}

auto FrameGraph::Reorder() -> Result<std::vector<CommandIndex> *> {
  ZoneScoped;

  // indices pointing to commands
  static std::vector<CommandIndex> commandsStack;
  snap_defer(commandsStack.clear());
  commandsStack.reserve(commands.size() / 4);

  for (const auto commandID : commands) {
    const auto &command = GetCommand(commandID);
    const auto &parents = commandParents.at(command.id);

    [[likely]]
    if (!parents.empty()) {
      continue;
    }

    assert(command.id == GetCommand(commands.at(command.id)).id);

    commandsStack.emplace_back(command.id);
  }

  static std::unordered_set<CommandIndex> visited;
  snap_defer(visited.clear());

  static std::vector<CommandIndex> reordered;
  reordered.clear();
  reordered.reserve(commands.size());

  while (!commandsStack.empty()) {
    const auto commandID = commandsStack.back();

    reordered.emplace_back(commandID);
    visited.emplace(commandID);
    auto nextCommands = PickNextCommands(commandID);
    commandsStack.pop_back();

    [[unlikely]]
    if (nextCommands.empty()) {
      continue;
    }

    Utils::UnorderedErase(nextCommands,
                          [&](const CommandIndex commandID) -> bool {
                            return visited.contains(commandID);
                          });

    commandsStack.append_range(nextCommands);
  }

  ERR_ASSERT(reordered.size() == commands.size());

  return &reordered;
}

auto FrameGraph::GetCommandLevel(CommandIndex commandID) -> CommandLevel {
  auto &command = GetCommand(commands.at(commandID));

  [[likely]]
  if (command.level != InvalidDepth) {
    return command.level;
  }

  command.level = 0U;

  for (const CommandIndex parent : commandParents.at(commandID)) {
    CommandLevel parentLevel = GetCommandLevel(parent);

    assert(parentLevel != InvalidDepth);

    command.level = std::max<CommandLevel>(command.level, parentLevel + 1U);
  }

#ifndef NDEBUG
  if (!commandParents.at(commandID).empty()) {
    assert(command.level != 0U);
  }
#endif

  return command.level;
}

auto FrameGraph::UpdateLevels(const std::vector<CommandIndex> &reordered)
    -> Error {
  ZoneScoped;

  for (auto commandID : commands) {
    auto &command = GetCommand(commandID);
    command.level = InvalidDepth;
  }

  graph.clear();

  for (auto commandID : reordered) {
    const CommandLevel level = GetCommandLevel(commandID);
    if (graph.size() <= level) {
      graph.resize(level + 1);
    }

    graph.at(level).commands.emplace_back(commandID);
    graph.at(level).level = level;
  }

  return {};
}

auto FrameGraph::BuildLoadOpModes() -> void {
  ZoneScoped;

  Math::StackVector<RenderState::RenderTarget, MAX_COLOR_ATTACHMENTS>
      lastColorAttachments;
  ObjectID lastDepthStencilAttachment = UINT64_MAX;
  bool isFirstIteration = true;

  for (const auto commandID : commands) {
    const auto &command = GetCommand(commandID);
    const auto type = command.GetType();

    if (type != CommandType::renderPass &&
        type != CommandType::vkCmdClearAttachments) {
      continue;
    }

    uint32_t stateID{};

    if (type == CommandType::renderPass) {
      const auto &renderPass = std::get<RenderPass>(command.data);
      stateID = renderPass.stateID;
    } else {
      stateID = command.GetDrawState()->stateID;
    }

    const GraphState &graphState = CommandStateManager::States.at(stateID);

    bool sameColorAttachments =
        lastColorAttachments == graphState.colorAttachments;

    bool sameDepthStencil = true;

    if (graphState.hasDepthStencilAttachment) {
      sameDepthStencil = lastDepthStencilAttachment ==
                         graphState.depthStencilAttachment.texture->getID();
    } else {
      sameDepthStencil = lastDepthStencilAttachment == UINT64_MAX;
    }

    bool attachmentsChanged = !sameColorAttachments || !sameDepthStencil;

    // Force Load Op Load if we aren't on the first iteration, and if the attachments were the same.
    loadOpConfigs.emplace(
        command.id, LoadOpConfig::FromGraphState(
                        graphState, !isFirstIteration && !attachmentsChanged));
    isFirstIteration = false;

    lastColorAttachments = graphState.colorAttachments;

    if (graphState.hasDepthStencilAttachment) {
      lastDepthStencilAttachment =
          graphState.depthStencilAttachment.texture->getID();
    } else {
      lastDepthStencilAttachment = UINT64_MAX;
    }
  }
}

inline auto GetCachedRenderPass(uint32_t stateID) -> RenderPass & {
  static std::vector<RenderPass> passCache{};
  static uint64_t lastFrameIdx = UINT64_MAX;
  static int currentCacheIdx = 0;
  if (lastFrameIdx != GetCurrentGraphicsContext()->currentFrame) {
    currentCacheIdx = 0;
  }

  lastFrameIdx = GetCurrentGraphicsContext()->currentFrame;

  if (currentCacheIdx >= passCache.size()) {
    currentCacheIdx++;
    return passCache.emplace_back(RenderPass{.stateID = stateID});
  }

  auto &pass = passCache.at(currentCacheIdx);
  currentCacheIdx++;

  pass.stateID = stateID;
  pass.commands.clear();
  pass.accesses.clear();
  pass.reads.clear();
  pass.writes.clear();

  return pass;
}

auto FrameGraph::CompactRenderPasses() -> Error {
  ZoneScoped;
  bool inRange{};

  // Amount of items merged, excluding one per range since the range object becomes a command
  int count = 0;

  CommandID currentPass = InvalidCommandID;

  inRange = false;
  int drawStateMismatches = 0;

  auto commandCount = commandIndices.size();
  assert(commandCount <= UINT16_MAX);

  ERR_ASSERT(commands.empty());
  ERR_ASSERT(renderPasses.empty());

  VirtualCommandBuffer *lastCmdBuffer = commandBuffers.back();
  // Idx = actual index + 1 to allow checking for invalid value at 0
  uint8_t lastCmdBufferIdx = commandBuffers.size();

  // NOLINTNEXTLINE
  for (CommandID commandID = 0; commandID < commandCount; commandID++) {
    const auto &command = GetCommand(commandID);
    const auto type = command.GetType();

    ERR_ASSERT(type != CommandType::renderPass);

    if (type == CommandType::vkCmdDraw ||
        type == CommandType::vkCmdDrawIndexed ||
        type == CommandType::vkCmdDrawIndexedIndirect ||
        type == CommandType::vkCmdDrawIndirect) {
      uint32_t stateID = command.GetDrawState()->stateID;

      if (!inRange) {
        auto *renderpassCommand =
            lastCmdBuffer->GetNewCommand(CommandType::renderPass);
        assert(renderpassCommand != nullptr);
        assert(renderpassCommand->GetType() == CommandType::renderPass);

        // Point the reordered command to the last index of commandIndices

        auto newIdx = commandIndices.size();
        commands.emplace_back(newIdx);
        renderPasses.emplace_back(newIdx);
        currentPass = newIdx;

        // Create the new command pointer in the last command buffer's commands, pointing to the last index
        commandIndices.emplace_back(lastCmdBuffer->commands.back(),
                                    lastCmdBufferIdx);

        auto &passCommand = GetCommand(currentPass);
        auto &passData = std::get<RenderPass>(passCommand.data);

        passData.reads.clear();
        passData.writes.clear();
        passData.accesses.clear();
        passData.commands.clear();
        passData.stateID = stateID;
      }
      inRange = true;

      auto &passCommand = GetCommand(currentPass);
      auto &passData = std::get<RenderPass>(passCommand.data);

      if (passData.stateID != stateID) {
        // Create new command in the last command buffer
        auto *renderpassCommand =
            lastCmdBuffer->GetNewCommand(CommandType::renderPass);
        assert(renderpassCommand != nullptr);
        assert(renderpassCommand->GetType() == CommandType::renderPass);

        // Point the reordered command to the last index of commandIndices
        auto newIdx = commandIndices.size();
        commands.emplace_back(newIdx);
        renderPasses.emplace_back(newIdx);
        currentPass = newIdx;

        // Create the new command pointer in the last command buffer's commands, pointing to the last index
        commandIndices.emplace_back(lastCmdBuffer->commands.back(),
                                    lastCmdBufferIdx);

        passCommand = GetCommand(currentPass);
        passData = std::get<RenderPass>(passCommand.data);

        passData.reads.clear();
        passData.writes.clear();
        passData.accesses.clear();
        passData.commands.clear();
        passData.stateID = stateID;

        drawStateMismatches++;
      }

      const auto *bound =
          get_if_derived<BoundResources>(GetCommand(commandID).data);

      passData.commands.emplace_back(commandID);
      passData.reads.append_range(bound->reads);
      passData.writes.append_range(bound->writes);
    } else {
      currentPass = InvalidCommandID;

      inRange = false;
      commands.emplace_back(commandID);
    }
  }

  CommandIndex idx{};

  {
    ZoneScopedN("De-Duplicate resources");
    for (const CommandID commandID : commands) {
      auto &command = GetCommand(commandID);

      command.id = idx++;
      command.uniqueID = command.id + commandIndices.size();
      assert(idx != InvalidCommandID);

      auto *bound = get_if_derived<BoundResources>(command.data);

      if (bound == nullptr) {
        continue;
      }

      Utils::DeDuplicate(bound->reads, std::less<VulkanResource>{},
                         &VulkanResource::operator==);
      Utils::DeDuplicate(bound->writes, std::less<VulkanResource>{},
                         &VulkanResource::operator==);
    }
  }

  // ERR_ASSERT(commands.size() <= commandIndices.size());
  // ERR_ASSERT(idx == commands.size());

  return {};
}

// NOLINTNEXTLINE
auto FrameGraph::Compile(const GraphicsContext &context) -> Error {
  ZoneScoped;

  CHECK_ERR(MapResourceUsages());
  CHECK_ERR(CompactRenderPasses());

  for (const auto &cmdBuffer : commandBuffers) {
    cmdBuffer->threadInfo.StartCommandWriting();
  }

  // for (const auto &cmdBuffer : commandBuffers) {
  //   cmdBuffer->threadInfo.FinalizeCommandWriting();
  // }

  CHECK_ERR(BuildGraph());
  CHECK_ERR(BuildReadyState());

  const auto *reordered = CHECK_RES(Reorder());
  CHECK_ERR(UpdateLevels(*reordered));

  for (const auto &cmdBuffer : commandBuffers) {
    cmdBuffer->threadInfo.FinalizeCommandWriting();
  }

  for (const CommandID commandID : renderPasses) {
    auto &command = GetCommand(commandID);
    assert(command.GetType() == CommandType::renderPass);

    auto &renderPass = std::get<RenderPass>(command.data);

    for (const CommandID childCmdID : renderPass.commands) {
      const auto &childCommand = GetCommand(childCmdID);
      const auto &bound = get_if_derived<BoundResources>(childCommand.data);

      for (const auto &access : bound->accesses) {
        renderPass.accesses.emplace(access);
      }
    }
  }

  CHECK_ERR(InsertBarriers());

  // PrintAlways("Command count: {}", commands.size());
  // PrintAlways("Level count: {}", graph.size());

  const auto &regions = CHECK_RES(BuildRenderRegions(context));

#if OUTPUT_DEBUG_GRAPH
  CHECK_ERR(DebugOutput());
#endif

  // {
  //   return Error::Create("Test");
  // }

  return {};
}

auto FrameGraph::WriteCommand(const Command &command,
                              const GraphicsContext &context,
                              VkCommandBuffer cmdBuffer,
                              const LoadOpConfig *loadOpConfig) -> Error {
  const auto *callable = get_if_derived<Callable>(command.data);

  if (callable == nullptr) {
    return {};
  }

  const auto type = command.GetType();
  if (GetRequiresRendering(type)) {
    const auto *state = command.GetDrawState();
    assert(state != nullptr);
    CHECK_ERR(state->Apply(context, cmdBuffer, loadOpConfig));
  } else {
    EndRendering(context, cmdBuffer);
  }

  CHECK_ERR(callable->Call(cmdBuffer));

  return {};
}

auto FrameGraph::Write(const GraphicsContext &context,
                       VkCommandBuffer cmdBuffer) -> Error {
  ZoneScoped;

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmdBuffer, &beginInfo);
  GetThreadContext().workingCommandBuffer = cmdBuffer;

  CHECK_ERR(DescriptorHeapManager::GetInstance().Bind(cmdBuffer));

  CHECK_ERR(BeginFrame(context));

  for (const auto &level : graph) {
    ZoneScopedN("Write level");

    if (!level.userBarriers.empty()) {
      EndRendering(context, cmdBuffer);

      for (const CommandIndex barrier : level.userBarriers) {
        const auto &command = GetCommand(barrier);
        const auto *callable = get_if_derived<Callable>(command.data);

        CHECK_ERR(callable->Call(cmdBuffer));
      }
    }

    std::vector<VkMemoryBarrier2> barriers = level.barriers;

    if (GetIsCurrentlyRendering()) {
      // Remove any color / depth attachment WaW barriers if we kept rendering.

      Utils::UnorderedErase(
          barriers, [](const VkMemoryBarrier2 &barrier) -> bool {
            constexpr auto colorOutput =
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            constexpr auto depthOutput =
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

            if (barrier.srcStageMask == colorOutput &&
                barrier.dstStageMask == colorOutput) {
              return true;
            }

            if ((barrier.srcStageMask & depthOutput) != 0U &&
                (barrier.dstStageMask & depthOutput) != 0U) {
              return true;
            }

            return false;
          });
    }

    if (!barriers.empty()) {
      EndRendering(context, cmdBuffer);

      VkDependencyInfo depInfo = {};
      depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      depInfo.memoryBarrierCount = barriers.size();
      depInfo.pMemoryBarriers = barriers.data();

      vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }

    for (const CommandIndex levelCmdID : level.commands) {
      const auto commandID = commands.at(levelCmdID);
      const auto &command = GetCommand(commandID);
      const auto type = command.GetType();

      if (type == CommandType::renderPass) {
        ZoneScopedN("Write render pass");
        const auto *loadOpConfig = &loadOpConfigs.at(levelCmdID);
        const auto &renderPass = std::get<RenderPass>(command.data);
        const auto &levelCommands = renderPass.commands;

        for (const CommandID childCommandId : levelCommands) {
          const auto &childCommand = GetCommand(childCommandId);
          CHECK_ERR(
              WriteCommand(childCommand, context, cmdBuffer, loadOpConfig));
        }
      } else {
        auto configIter = loadOpConfigs.find(levelCmdID);
        if (configIter == loadOpConfigs.end()) {
          CHECK_ERR(WriteCommand(command, context, cmdBuffer, nullptr));
        } else {
          CHECK_ERR(
              WriteCommand(command, context, cmdBuffer, &configIter->second));
        }
      }
    }
  }

  EndRendering(context, cmdBuffer);
  GetThreadContext().workingCommandBuffer = nullptr;
  vkEndCommandBuffer(cmdBuffer);

  Reset();

  return {};
}

auto FrameGraph::Reset() -> void {
#if OUTPUT_DEBUG_GRAPH
  dependencies.clear();
#endif
  graph.clear();
  ancestorStack.clear();
  ancestorStamps.clear();
  commandParents.clear();
  commandHazardSources.clear();
  nextReady.clear();
  loadOpConfigs.clear();
  commands.clear();
  resourcesWritesInFrame.clear();
  renderPasses.clear();
  CommandStateManager::StateToIndex.clear();
  CommandStateManager::States.clear();
  RecordingState::CurrentState = {};
  RecordingState::LastStateStorage = {};
  RecordingState::LastState = nullptr;
  CommandStateManager::CurrentStateID = UINT32_MAX;
}

} // namespace Graphics