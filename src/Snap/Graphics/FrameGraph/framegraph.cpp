#include "framegraph.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/dynamicRendering.hpp"
#include "Graphics/FrameGraph/pipelineState.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include <cstddef>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#if OUTPUT_DEBUG_GRAPH
#include "Modules/filesystem.hpp"
#include <format>
#endif
#include "Modules/timer.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <slang/slang.h>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto FrameGraph::Submit(const GraphicsContext &context,
                        const VirtualCommandBuffer &commands) -> Error {
  commandBuffer = commands;

  CHECK_ERR(Compile(context));

  return {};
}

auto FrameGraph::ValidateGraph() -> Error { return {}; } // NOLINT

inline auto GetReadsFromDrawState(DrawState &state, bool getRendertargets)
    -> std::vector<VulkanResource> {
  std::vector<VulkanResource> reads;

  for (const auto &image : state.boundImages) {
    if (image.access != SLANG_RESOURCE_ACCESS_WRITE) {
      reads.emplace_back(image.resource);
    }
  }

  for (const auto &buffer : state.boundBuffers) {
    if (buffer.access != SLANG_RESOURCE_ACCESS_WRITE) {
      reads.emplace_back(buffer.resource);
    }
  }

  for (const auto &accel : state.boundASs) {
    reads.emplace_back(accel.resource);
  }

  if (!getRendertargets) {
    return reads;
  }

  for (const auto &vertexBuffer : state.vertexBuffers) {
    if (vertexBuffer != VK_NULL_HANDLE) {
      reads.emplace_back(vertexBuffer);
    }
  }

  if (state.indexBuffer != VK_NULL_HANDLE) {
    reads.emplace_back(state.indexBuffer);
  }

  for (const auto &colorAttachment : state.colorAttachments) {
    reads.emplace_back(colorAttachment.resource);
  }

  if (state.depthStencilAttachment.has_value()) {
    reads.emplace_back(state.depthStencilAttachment->resource);
  }

  return reads;
}

inline auto GetReadsInternal(Command &command) -> std::vector<VulkanResource> {
  auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    bool getRendertargets =
        drawState->GetGraphState().bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;

    return GetReadsFromDrawState(*drawState, getRendertargets);
  }

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    return args.srcResources;
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    return {
        VulkanResource(ImageSubresource(args.texture->imageMemory->image, 0, 1,
                                        0, args.texture->GetMipmapCount()))};
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    return {args.srcBuffer};
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    return args.srcResources;
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    return {args.srcBuffer};
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    return args.srcResources;
  }

  if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
          command.data)) {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    return args.reads;
  }

  if (std::holds_alternative<Args::VkCmdPipelineBarrier2>(command.data)) {
    const auto &args = std::get<Args::VkCmdPipelineBarrier2>(command.data);
    std::vector<VulkanResource> resources{};
    resources.reserve(args.imageMemoryBarriers.size() +
                      args.bufferMemoryBarriers.size());

    for (const auto &barrier : args.imageMemoryBarriers) {
      resources.emplace_back(
          ImageSubresource(barrier.image, barrier.subresourceRange));
    }

    for (const auto &barrier : args.bufferMemoryBarriers) {
      resources.emplace_back(barrier.buffer);
    }

    return resources;
  }

  return {};
}

inline auto GetWritesFromDrawState(DrawState &state, bool getRendertargets)
    -> std::vector<VulkanResource> {
  std::vector<VulkanResource> writes;

  for (const auto &buffer : state.boundBuffers) {
    if (buffer.access == SLANG_RESOURCE_ACCESS_WRITE ||
        buffer.access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
      writes.emplace_back(buffer.resource);
    }
  }

  for (const auto &image : state.boundImages) {
    if (image.access == SLANG_RESOURCE_ACCESS_WRITE ||
        image.access == SLANG_RESOURCE_ACCESS_READ_WRITE) {
      writes.emplace_back(image.resource);
    }
  }

  if (!getRendertargets) {
    return writes;
  }

  for (const auto &colorAttachment : state.colorAttachments) {
    writes.emplace_back(colorAttachment.resource);
  }

  if (state.depthStencilAttachment.has_value()) {
    writes.emplace_back(state.depthStencilAttachment->resource);
  }

  return writes;
}

inline auto GetWritesInternal(Command &command) -> std::vector<VulkanResource> {
  auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    bool getRendertargets =
        drawState->GetGraphState().bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS;
    getRendertargets = getRendertargets ||
                       command.GetType() == CommandType::vkCmdClearAttachments;

    return GetWritesFromDrawState(*drawState, getRendertargets);
  }

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    return args.dstResources;
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    return {
        VulkanResource(ImageSubresource(args.texture->imageMemory->image, 0, 1,
                                        0, args.texture->GetMipmapCount()))};
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    return {args.dstBuffer};
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    return args.dstResources;
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    return args.dstResources;
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    return {args.dstBuffer};
  }

  if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    return {args.dstBuffer};
  }

  if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
          command.data)) {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    return args.writes;
  }

  if (std::holds_alternative<Args::VkCmdPipelineBarrier2>(command.data)) {
    const auto &args = std::get<Args::VkCmdPipelineBarrier2>(command.data);
    std::vector<VulkanResource> resources{};
    resources.reserve(args.imageMemoryBarriers.size() +
                      args.bufferMemoryBarriers.size());

    for (const auto &barrier : args.imageMemoryBarriers) {
      resources.emplace_back(
          ImageSubresource(barrier.image, barrier.subresourceRange));
    }

    for (const auto &barrier : args.bufferMemoryBarriers) {
      resources.emplace_back(barrier.buffer);
    }

    return resources;
  }

  // if (std::holds_alternative<Args::VkCmdClearAttachments>(command.data)) {
  //   const auto &args = std::get<Args::VkCmdClearAttachments>(command.data);
  // }

  return {};
}

auto FrameGraph::MapResourceUsages() -> Error {
  ZoneScoped;

  Utils::ParallelFor(
      commandBuffer.commands.size() * 2, [this](size_t commandIdx) -> void {
        Command &command = commandBuffer.commands.at(commandIdx / 2);
        auto *boundState = get_if_derived<BoundResources>(command.data);

        if (boundState == nullptr) {
          return;
        }

        if (commandIdx & 1UL) { // Odd
          auto reads = std::move(GetReadsInternal(command));

          std::ranges::sort(reads, std::less{});
          auto [first, last] =
              std::ranges::unique(reads, VulkanResource::Equals);
          reads.erase(first, last);
          boundState->reads = std::move(reads);
        } else {
          auto writes = std::move(GetWritesInternal(command));

          std::ranges::sort(writes, std::less{});
          auto [first, last] =
              std::ranges::unique(writes, VulkanResource::Equals);
          writes.erase(first, last);
          boundState->writes = std::move(writes);
        }
      });

  {
    ZoneScopedN("Map writes");

    for (const auto &command : commandBuffer.commands) {
      const auto *boundState = get_if_derived<BoundResources>(command.data);

      if (boundState == nullptr) {
        continue;
      }

      for (const auto &write : boundState->writes) {
        resourcesWritesInFrame.emplace(write);
      }
    }
  }

  {
    ZoneScopedN("Erase irrelevant data");
    Utils::ParallelFor(
        commandBuffer.commands, [this](Command &command) -> void {
          auto *boundState = get_if_derived<BoundResources>(command.data);

          if (boundState == nullptr) {
            return;
          }

          Utils::UnorderedErase(boundState->reads,
                                [this](const VulkanResource &resource) -> bool {
                                  return !resourcesWritesInFrame.contains(
                                      resource);
                                });
        });
  }

  return {};
}

// Marks every strict ancestor of `from` with `stamp`, walking the already
// reduced parent edges. Commands older than `floor` are never expanded: any
// path between two candidate parents only passes through commands younger than
// the oldest candidate, so cutting there does not change the outcome.
auto FrameGraph::MarkAncestors(CommandID from, CommandID floor, uint32_t stamp)
    -> void {
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
auto FrameGraph::ReduceParents(CommandID idx,
                               const std::vector<CommandID> &candidates)
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
    CommandID lastWrite = InvalidCommandID;
    std::vector<CommandID> readsSinceWrite;
  };

  // Resources only ever overlap when they share the same underlying handle
  // (same VkImage, VkBuffer, or acceleration structure) -- different handles
  // never alias. So bucket frontiers by handle: overlap/equality scans then
  // only cover the sub-ranges of one resource (a handful of mips/layers at
  // most) instead of every resource touched anywhere in the command buffer.
  struct ResourceKey {
    VulkanResource::ResourceType type;
    const void *handle;

    auto operator==(const ResourceKey &other) const -> bool {
      return type == other.type && handle == other.handle;
    }
  };

  struct ResourceKeyHash {
    auto operator()(const ResourceKey &key) const -> size_t {
      Hash::Hasher hasher;
      hasher.Add(static_cast<uint32_t>(key.type));
      hasher.Add(key.handle);
      return hasher.Get();
    }
  };

  const auto KeyOf = [](const VulkanResource &resource) -> ResourceKey {
    switch (resource.type) {
    case VulkanResource::ResourceType::Image:
      return {.type = resource.type,
              .handle = static_cast<const void *>(resource.image.image)};
    case VulkanResource::ResourceType::Buffer:
      return {.type = resource.type,
              .handle = static_cast<const void *>(resource.buffer)};
    case VulkanResource::ResourceType::AccelerationStructure:
      return {.type = resource.type,
              .handle =
                  static_cast<const void *>(resource.accelerationStructure)};
    }
    assert(false && "Unreachable");
    return {};
  };

  std::unordered_map<ResourceKey,
                     std::vector<std::pair<VulkanResource, Frontier>>,
                     ResourceKeyHash>
      frontiers;
  std::vector<CommandID> candidates;

  for (auto &command : commands) {
    // Hot path. 800/1200 microseconds for large calls (depth prepass & material pass)
    ZoneScopedN("Determine command parents");

    candidates.clear();

    for (const auto &resource : GetReads(command)) { // RAW
      if (!resourcesWritesInFrame.contains(resource)) {
        continue;
      }

      const auto bucketIt = frontiers.find(KeyOf(resource));
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
      const auto bucketIt = frontiers.find(KeyOf(resource));
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

    std::ranges::sort(candidates, std::greater{});
    {
      auto [first, last] = std::ranges::unique(candidates);
      candidates.erase(first, last);
    }

    CommandLevel level = 0;
    for (const auto parent : candidates) {
      const auto parentLevel = commands[parent].level;
      ERR_ASSERT(parentLevel != InvalidDepth);

      level = std::max<CommandLevel>(level, parentLevel + 1);
    }

    command.level = level;
    commandHazardSources[command.id] = candidates;

    ReduceParents(command.id, candidates);

    for (const auto &resource : GetReads(command)) {
      if (!resourcesWritesInFrame.contains(resource)) {
        continue;
      }

      auto &bucket = frontiers[KeyOf(resource)];

      bool found = false;
      for (auto &[existing, frontier] : bucket) {
        if (!VulkanResource::Equals(existing, resource)) {
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

    // Done after the reads so a read-write resource does not list this
    // command as its own parent on the next usage.
    for (const auto &resource : GetWrites(command)) {
      auto &bucket = frontiers[KeyOf(resource)];

      bool found = false;
      for (auto &[existing, frontier] : bucket) {
        if (!VulkanResource::Equals(existing, resource)) {
          continue;
        }

        frontier.lastWrite = command.id;
        frontier.readsSinceWrite.clear();
        found = true;
        break;
      }
      if (!found) {
        bucket.emplace_back(resource, Frontier{.lastWrite = command.id});
      }
    }
  }

  return {};
}

auto FrameGraph::BuildGraph() -> Error {
  ZoneScoped;

  auto time = Timer::GetTime();
  CHECK_ERR(PreCompile());
  auto time2 = Timer::GetTime();

  CommandLevel maxLevel{};

  for (const auto &command : commands) {
    maxLevel = std::max(maxLevel, command.level);
  }

  graph.resize(maxLevel + 1);
  for (auto i = 0; i < graph.size(); i++) {
    graph.at(i).level = i;
  }

  size_t edgeCount = 0;
  for (const auto &command : commands) {
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
    stream << std::format("subgraph cluster_level_{} {{\n", level.level);
    stream << std::format("label = \"Level {}\";\n", level.level);

    for (const auto commandId : level.commands) {
      const auto &command = commands.at(commandId);
      if (ExportRWLabels) {
        const auto &reads = GetReads(command);
        const auto &writes = GetWrites(command);

        std::stringstream readStr;
        std::stringstream writeStr;

        for (const auto &resource : reads) {
          readStr << resource.ToString();

          if (!VulkanResource::Equals(resource, reads.back())) {
            readStr << "\n";
          }
        }

        for (const auto &resource : writes) {
          writeStr << resource.ToString();

          if (!VulkanResource::Equals(resource, writes.back())) {
            writeStr << "\n";
          }
        }

        stream << std::format("{} [label=\"{}: {}\nR: {}\nW: {}\"];\n",
                              command.id, command.level,
                              CommandTypeEnumHelper.ToString(command.GetType()),
                              readStr.str(), writeStr.str());
      } else {
        stream << std::format(
            "{} [label=\"{}: {}\"];\n", command.id, command.level,
            CommandTypeEnumHelper.ToString(command.GetType()));
      }
    }

    stream << std::format("barriers_{} [\n", level.level);
    // for (const auto &barrier : level.barriers) {
    // }
    stream << std::format("shape=note,\nlabel=\"{} Barriers\"\n",
                          level.barriers.size());
    stream << "];\n}\n";
  }

  for (const auto dependency : dependencies) {
    uint32_t parent = (dependency >> UINT16_WIDTH) & UINT16_MAX;
    uint32_t child = dependency & UINT16_MAX;

    ERR_ASSERT_MSG(
        commands.at(child).level > commands.at(parent).level,
        std::format(
            "child: {} with parent: {}, child level {} <= parent level {}",
            child, parent, commands.at(child).level,
            commands.at(parent).level));

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

  std::array<VkPipelineStageFlags2, UINT64_WIDTH> maskBits{};
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
  for (auto level = end; level >= start + 1; level--) {
    const auto &barriers = graph.at(level).barriers;

    for (const auto &barrier : barriers) {
      for (const auto bitIndex :
           Utils::BitIndexRange(barrier.dstStageMask & dstPipeline)) {
        // If this stage bit is active in this sync and we still need it
        const auto mask = 1ULL << bitIndex;

        const bool srcMatch =
            // Match the sync's source stage with our current bit
            (barrier.srcStageMask & lastWritePipeline) != 0U &&
            // Match the sync's source access with our resource's last usage
            (barrier.srcAccessMask & lastWriteAccess) != 0U;

        if (srcMatch) {
          // Remove the accesses that are now satisfied
          maskBits.at(bitIndex) &= ~barrier.dstAccessMask;

          // All synced
          if (maskBits.at(bitIndex) == 0UL) {
            // Remove this pipeline to be checked
            dstPipeline &= ~mask;

            // No more pipelines to be checked, early return.
            if (dstPipeline == 0) {
              return maskBits;
            }
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

inline auto IsHazard(const VkAccessFlags2 srcAccess,
                     const VkAccessFlags2 dstAccess) -> bool {
  static constexpr VkAccessFlagBits2 writeAccessBits =
      VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT |
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
      VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

  return ((srcAccess | dstAccess) & writeAccessBits) != 0U;
}

auto FrameGraph::GetRequiredBarriers(
    CommandID commandId, const VulkanResource &resource,
    VkAccessFlags2 accesses, VkPipelineStageFlags2 pipelines,
    std::vector<VkMemoryBarrier2> &memoryBarriers) -> void {
  const auto &command = commands[commandId];

  if (command.level == 0) {
    return;
  }

  // Walks every real hazard source for this resource, not just the
  // transitively-reduced commandParents: a source dropped there for being
  // reachable through another kept candidate can still be the only command
  // carrying the write access/stage info this barrier needs.
  const auto &hazardSources = commandHazardSources[commandId];

  for (const auto &parentID : hazardSources) {
    const auto &parent = commands[parentID];
    const auto [srcAccess, srcStage] = ResourceWritesAt(parentID, resource);

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
      const auto unsyncedAccesses = mask.at(bit);

      if (unsyncedAccesses == 0U || !IsHazard(unsyncedAccesses, srcAccess)) {
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

// NOLINTNEXTLINE
auto FrameGraph::ResourceAccessAt(CommandID commandId,
                                  const VulkanResource &resource)
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  const auto &command = commands.at(commandId);

  const auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    const auto &pair = drawState->GetStateFor(resource, command.GetType());
    if (pair.first != 0U && pair.second != 0U) {
      return pair;
    }
  }

  std::pair<VkAccessFlags2, VkPipelineStageFlags2> flags{};
  const auto addToFlags =
      [&flags](const std::pair<VkAccessFlags2, VkPipelineStageFlags2> &flag)
      -> void {
    flags.first |= flag.first;
    flags.second |= flag.second;
  };

  const auto overlaps = [](const std::vector<VulkanResource> &resources,
                           const VulkanResource &resource) -> bool {
    return std::ranges::any_of(resources, [&](const auto &other) -> auto {
      return other.Overlaps(resource);
    });
  };

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);

    if (overlaps(args.srcResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
    }
    if (overlaps(args.dstResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
    }
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    addToFlags(
        {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    if (resource.Overlaps(args.srcBuffer)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
    if (resource.Overlaps(args.dstBuffer)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    if (overlaps(args.srcResources, resource)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
    if (overlaps(args.dstResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    if (resource.Overlaps(args.srcBuffer)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
    if (overlaps(args.dstResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    if (overlaps(args.srcResources, resource)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
    if (resource.Overlaps(args.dstBuffer)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    if (resource.Overlaps(args.dstBuffer)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
          command.data)) {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    for (const auto &read : args.reads) {
      if (resource.Overlaps(read)) {
        addToFlags({VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR});
      }
    }
    for (const auto &write : args.writes) {
      if (resource.Overlaps(write)) {
        addToFlags({VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR});
      }
    }
  }

  return flags;
}

// NOLINTNEXTLINE
auto FrameGraph::ResourceReadsAt(CommandID commandId,
                                 const VulkanResource &resource)
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  const auto &command = commands.at(commandId);

  const auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    const auto &pair = drawState->GetReadStateFor(resource, command.GetType());
    if (pair.first != 0U && pair.second != 0U) {
      return pair;
    }
  }

  std::pair<VkAccessFlags2, VkPipelineStageFlags2> flags{};
  const auto addToFlags =
      [&flags](const std::pair<VkAccessFlags2, VkPipelineStageFlags2> &flag)
      -> void {
    flags.first |= flag.first;
    flags.second |= flag.second;
  };

  const auto overlaps = [](const std::vector<VulkanResource> &resources,
                           const VulkanResource &resource) -> bool {
    return std::ranges::any_of(resources, [&](const auto &other) -> auto {
      return other.Overlaps(resource);
    });
  };

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    if (overlaps(args.srcResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
    }
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    addToFlags(
        {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    if (resource.Overlaps(args.srcBuffer)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    if (overlaps(args.srcResources, resource)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    if (resource.Overlaps(args.srcBuffer)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    if (overlaps(args.srcResources, resource)) {
      addToFlags({VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
          command.data)) {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    for (const auto &read : args.reads) {
      if (resource.Overlaps(read)) {
        addToFlags({VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR});
      }
    }
  }

  return flags;
}

// NOLINTNEXTLINE
auto FrameGraph::ResourceWritesAt(CommandID commandId,
                                  const VulkanResource &resource)
    -> std::pair<VkAccessFlags2, VkPipelineStageFlags2> {
  const auto &command = commandBuffer.commands.at(commandId);

  const auto *drawState = get_if_derived<DrawState>(command.data);

  if (drawState != nullptr) {
    const auto &pair = drawState->GetWriteStateFor(resource, command.GetType());
    // if (pair.first != 0U && pair.second != 0U) {
    return pair;
    // }
  }

  std::pair<VkAccessFlags2, VkPipelineStageFlags2> flags{};
  const auto addToFlags =
      [&flags](const std::pair<VkAccessFlags2, VkPipelineStageFlags2> &flag)
      -> void {
    flags.first |= flag.first;
    flags.second |= flag.second;
  };

  const auto overlaps = [](const std::vector<VulkanResource> &resources,
                           const VulkanResource &resource) -> bool {
    return std::ranges::any_of(resources, [&](const auto &other) -> auto {
      return other.Overlaps(resource);
    });
  };

  if (std::holds_alternative<Args::VkCmdBlitImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdBlitImage>(command.data);
    if (overlaps(args.dstResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
    }
  }

  if (std::holds_alternative<Args::MipmapTexture>(command.data)) {
    const auto &args = std::get<Args::MipmapTexture>(command.data);
    addToFlags(
        {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
  }

  if (std::holds_alternative<Args::VkCmdCopyBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBuffer>(command.data);
    if (resource.Overlaps(args.dstBuffer)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImage>(command.data);
    if (overlaps(args.dstResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyBufferToImage>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyBufferToImage>(command.data);
    if (overlaps(args.dstResources, resource)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdCopyImageToBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdCopyImageToBuffer>(command.data);
    if (resource.Overlaps(args.dstBuffer)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COPY_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdFillBuffer>(command.data)) {
    const auto &args = std::get<Args::VkCmdFillBuffer>(command.data);
    if (resource.Overlaps(args.dstBuffer)) {
      addToFlags(
          {VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT});
    }
  }

  if (std::holds_alternative<Args::VkCmdBuildAccelerationStructuresKHR>(
          command.data)) {
    const auto &args =
        std::get<Args::VkCmdBuildAccelerationStructuresKHR>(command.data);
    for (const auto &write : args.writes) {
      if (resource.Overlaps(write)) {
        addToFlags({VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
                    VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR});
      }
    }
  }

  return flags;
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
          level.commands, [&](const CommandID &commandId) -> bool {
            const auto &command = commands[commandId];

            if (command.GetType() == CommandType::vkCmdPipelineBarrier2) {
              level.userBarriers.emplace_back(command.id);

              return true;
            }
            return false;
          });
    }
  }

  const auto addBarriers = [this](const std::vector<VulkanResource> &resources,
                                  const CommandID commandId,
                                  Graphics::Level &level) -> void {
    for (const auto &resource : resources) {
      if (!resourcesWritesInFrame.contains(resource)) {
        continue;
      }

      const auto &[accesses, pipelines] = ResourceAccessAt(commandId, resource);
      GetRequiredBarriers(commandId, resource, accesses, pipelines,
                          level.barriers);
    }
  };

  for (auto &level : graph) {
    ZoneScopedN("level barriers");
    for (const auto commandId : level.commands) {
      const auto &command = commands[commandId];

      // if (command.GetType() == CommandType::renderPass) {

      // } else {
      addBarriers(GetReads(command), commandId, level);
      addBarriers(GetWrites(command), commandId, level);
      // }
    }

    MergeBarriers(level.barriers);
  }

  return {};
}

inline auto GetRenderExtent(const Graphics::GraphState &state) -> VkExtent2D {

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
      const auto &command = commands.at(commandID);
      const auto *drawState = command.GetDrawState();
      const auto *callableState = get_if_derived<Callable>(command.data);
      const auto type = command.GetType();

      // Any non-draw command breaks the current contiguous rendering region.
      if ((drawState == nullptr || callableState == nullptr ||
           !callableState->requiresRendering) &&
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

      auto &renderState = CommandStateManager::States.at(stateID);

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

    for (const auto &command : commands) {
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
      for (const auto &command : commands) {
        if (scheduled[command.id]) {
          continue;
        }

        if (std::find(commandParents[command.id].begin(),
                      commandParents[command.id].end(),
                      commandID) == commandParents[command.id].end()) {
          continue;
        }

        const bool parentsReady = std::all_of(
            commandParents[command.id].begin(),
            commandParents[command.id].end(),
            [&](CommandID parent) -> auto { return scheduled[parent]; });

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

auto FrameGraph::ScoreCommand(CommandID parentID, CommandID childID)
    -> uint32_t {
  const auto &parent = commands.at(parentID);
  const auto &child = commands.at(childID);

  const auto DynamicStateUpdateCost = 100U;
  const auto EndRenderingCost = 500U;
  const auto StartRenderingCost = 1000U;

  uint32_t cost = 0U;

  const auto *parentDrawState = parent.GetDrawState();
  const auto *childDrawState = child.GetDrawState();

  if (parentDrawState != nullptr) { // Parent is rendering
    const auto &graphState = parentDrawState->GetGraphState();

    if (childDrawState != nullptr) { // Child is rendering
      const auto &childGraphState = childDrawState->GetGraphState();

      if (!ComparePipelineCompatability(graphState, childGraphState)) {
        cost += EndRenderingCost;

        if (childGraphState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
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
    if (childGraphState.bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      cost += StartRenderingCost;
    }
  } else { // Neither child nor parent are rendering
    // no extra cost to call
  }

  return cost;
}

// returns a list of commands, with the best pick last
auto FrameGraph::PickNextCommands(CommandID parent) -> std::vector<CommandID> {
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
  CommandID bestCommand = children.front();

  static std::unordered_map<CommandID, uint32_t> scores;
  snap_defer(scores.clear());
  scores.reserve(children.size());

  for (CommandID child : children) {
    scores.emplace(child, ScoreCommand(parent, child));
  }

  std::vector<CommandID> sortedChildren{children};

  std::ranges::sort(
      sortedChildren,
      [](const CommandID &first, const CommandID &second) -> bool {
        // A(3) > B(2) -> A, B. We want highest score first, so A > B
        return scores.at(first) > scores.at(second);
      });

  return sortedChildren;
}

auto FrameGraph::Reorder() -> Result<std::vector<CommandID>> {
  ZoneScoped;

  static std::vector<CommandID> commandsStack;
  snap_defer(commandsStack.clear());
  commandsStack.reserve(commands.size() / 4);

  for (const auto &command : commands) {
    const auto &parents = commandParents.at(command.id);

    [[likely]]
    if (!parents.empty()) {
      continue;
    }

    commandsStack.emplace_back(command.id);
  }

  static std::unordered_set<CommandID> visited;
  snap_defer(visited.clear());

  std::vector<CommandID> reordered;
  reordered.reserve(commands.size());

  while (!commandsStack.empty()) {
    reordered.emplace_back(commandsStack.back());
    visited.emplace(commandsStack.back());
    auto nextCommands = PickNextCommands(commandsStack.back());
    commandsStack.pop_back();

    [[unlikely]]
    if (nextCommands.empty()) {
      continue;
    }

    Utils::UnorderedErase(nextCommands,
                          [&](const CommandID &commandId) -> bool {
                            return visited.contains(commandId);
                          });

    commandsStack.append_range(nextCommands);
  }

  ERR_ASSERT(reordered.size() == commands.size());

  return reordered;
}

auto FrameGraph::GetCommandLevel(CommandID commandId) -> CommandLevel {
  auto &command = commands.at(commandId);

  [[likely]]
  if (command.level != InvalidDepth) {
    return command.level;
  }

  command.level = 0U;

  for (const CommandID parent : commandParents.at(commandId)) {
    CommandLevel parentLevel = GetCommandLevel(parent);

    assert(parentLevel != InvalidDepth);

    command.level = std::max<CommandLevel>(command.level, parentLevel + 1U);
  }

  if (!commandParents.at(commandId).empty()) {
    assert(command.level != 0U);
  }

  return command.level;
}

auto FrameGraph::UpdateLevels(const std::vector<CommandID> &reordered)
    -> Error {
  ZoneScoped;

  for (auto &command : commands) {
    command.level = InvalidDepth;
  }

  for (auto commandId : reordered) {
    GetCommandLevel(commandId);
  }

  CommandLevel maxLevel = 0U;
  for (const CommandID commandId : reordered) {
    const auto &command = commands.at(commandId);
    ERR_ASSERT(command.level != InvalidDepth);
    maxLevel = std::max(maxLevel, command.level);
  }

  size_t startSize = graph.size();
  graph.clear();
  graph.resize(maxLevel + 1);

  for (auto i = 0; i < graph.size(); i++) {
    graph.at(i).level = i;
    graph.at(i).commands.clear();
  }

  for (const CommandID commandId : reordered) {
    auto &command = commands.at(commandId);
    graph.at(command.level).commands.emplace_back(commandId);
  }

  return {};
}

auto FrameGraph::BuildLoadOpModes() -> void {
  ZoneScoped;

  Math::StackVector<RenderState::RenderTarget, MAX_COLOR_ATTACHMENTS>
      lastColorAttachments;
  ObjectID lastDepthStencilAttachment = UINT64_MAX;
  bool isFirstIteration = true;

  for (const auto &command : commands) {
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

auto FrameGraph::CompactRenderPasses() -> Error {
  ZoneScoped;
  bool inRange{};

  // Amount of items merged, excluding one per range since the range object becomes a command
  int count = 0;

  for (const auto &command : commandBuffer.commands) {
    const auto type = command.GetType();

    if (type == CommandType::vkCmdDraw ||
        type == CommandType::vkCmdDrawIndexed ||
        type == CommandType::vkCmdDrawIndexedIndirect ||
        type == CommandType::vkCmdDrawIndirect) {
      if (!inRange) {
        inRange = true;
      } else {
        count++;
      }
    } else {
      inRange = false;
    }
  }

  commands.reserve(commandBuffer.commands.size() - count);

  if (count == 0) {
    PrintAlways("No render passes, skipping compacting.");
    commands = commandBuffer.commands;
    return {};
  }

  RenderPass currentPass;
  std::unordered_set<VulkanResource, VulkanResourceHash> knownReads;
  std::unordered_set<VulkanResource, VulkanResourceHash> knownWrites;
  inRange = false;
  int drawStateMismatches = 0;

  for (const auto &command : commandBuffer.commands) {
    const auto type = command.GetType();

    if (type == CommandType::vkCmdDraw ||
        type == CommandType::vkCmdDrawIndexed ||
        type == CommandType::vkCmdDrawIndexedIndirect ||
        type == CommandType::vkCmdDrawIndirect) {
      uint32_t stateID = command.GetDrawState()->stateID;

      if (!inRange) {
        currentPass = {.stateID = stateID};
      }
      inRange = true;

      if (currentPass.stateID != stateID) {
        currentPass.reads = {knownReads.begin(), knownReads.end()};
        currentPass.writes = {knownWrites.begin(), knownWrites.end()};
        commands.emplace_back(currentPass);
        currentPass = {.stateID = stateID};
        knownReads.clear();
        knownWrites.clear();
        drawStateMismatches++;
      }

      currentPass.commands.emplace_back(command.id);
      // currentPass.reads.append_range(GetReads(command));
      // currentPass.writes.append_range(GetWrites(command));
      for (const auto &read : GetReads(command)) {
        knownReads.emplace(read);
      }
      for (const auto &write : GetWrites(command)) {
        knownWrites.emplace(write);
      }
    } else {
      if (inRange) {
        currentPass.reads = {knownReads.begin(), knownReads.end()};
        currentPass.writes = {knownWrites.begin(), knownWrites.end()};
        commands.emplace_back(currentPass);
        knownReads.clear();
        knownWrites.clear();
      }

      inRange = false;
      commands.emplace_back(command);
    }
  }

  if (inRange) {
    currentPass.reads = {knownReads.begin(), knownReads.end()};
    currentPass.writes = {knownWrites.begin(), knownWrites.end()};
    commands.emplace_back(currentPass);
  }

  CommandID idx{};

  {
    ZoneScopedN("De-Duplicate resources");
    for (auto &command : commands) {
      command.id = idx++;
      ERR_ASSERT(idx != UINT16_MAX);

      auto *bound = get_if_derived<BoundResources>(command.data);

      if (bound == nullptr) {
        continue;
      }

      size_t readsBefore = bound->reads.size();
      size_t writesBefore = bound->writes.size();
      Utils::DeDuplicate(bound->reads, std::less<VulkanResource>{},
                         VulkanResource::Equals);
      Utils::DeDuplicate(bound->writes, std::less<VulkanResource>{},
                         VulkanResource::Equals);
    }
  }

  // PrintAlways(
  //     "Starting command count: {}, reduced: {}, draw state mismatches {}",
  //     commandBuffer.commands.size(), commands.size(), drawStateMismatches);

  return {};
}

// NOLINTNEXTLINE
auto FrameGraph::Compile(const GraphicsContext &context) -> Error {
  ZoneScoped;

  CHECK_ERR(ValidateGraph());

  CommandID idx = 0;

  for (auto &command : commandBuffer.commands) {
    command.id = idx++;
    ERR_ASSERT(idx != UINT16_MAX);
  }

  CHECK_ERR(MapResourceUsages());
  CHECK_ERR(CompactRenderPasses());
  CHECK_ERR(BuildGraph());
  CHECK_ERR(BuildReadyState());

  const auto &reordered = CHECK_RES(Reorder());
  CHECK_ERR(UpdateLevels(reordered));
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

auto WriteCommand(const std::vector<Command> &commands, CommandID commandId,
                  const GraphicsContext &context, VkCommandBuffer cmdBuffer,
                  const LoadOpConfig *loadOpConfig) -> Error {
  const auto &command = commands.at(commandId);
  const auto *callable = get_if_derived<Callable>(command.data);
  const auto *state = get_if_derived<DrawState>(command.data);

  if (callable != nullptr && callable->requiresRendering) {
    if (state != nullptr) {
      CHECK_ERR(state->Apply(context, cmdBuffer, loadOpConfig));
    }
  }

  if (callable != nullptr) {
    if (!callable->requiresRendering) {
      EndRendering(context, cmdBuffer);
    }
    CHECK_ERR(callable->Call(cmdBuffer));
  }

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

  CHECK_ERR(BeginFrame(context));

  for (const auto &level : graph) {
    ZoneScopedN("Write level");

    if (!level.userBarriers.empty()) {
      EndRendering(context, cmdBuffer);

      for (const auto &barrier : level.userBarriers) {
        const auto &command = commands.at(barrier);
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

    for (const CommandID commandId : level.commands) {
      const auto &command = commands.at(commandId);
      const auto type = command.GetType();

      if (type == CommandType::renderPass) {
        ZoneScopedN("Write render pass");
        const auto *loadOpConfig = &loadOpConfigs.at(commandId);
        const auto &renderPass = std::get<RenderPass>(command.data);
        const auto &levelCommands = renderPass.commands;

        for (const CommandID childCommandId : levelCommands) {
          CHECK_ERR(WriteCommand(commandBuffer.commands, childCommandId,
                                 context, cmdBuffer, loadOpConfig));
        }
      } else {
        auto configIter = loadOpConfigs.find(commandId);
        if (configIter == loadOpConfigs.end()) {
          CHECK_ERR(
              WriteCommand(commands, commandId, context, cmdBuffer, nullptr));
        } else {
          CHECK_ERR(WriteCommand(commands, commandId, context, cmdBuffer,
                                 &configIter->second));
        }
      }
    }
  }

  EndRendering(context, cmdBuffer);
  GetThreadContext().workingCommandBuffer = nullptr;
  vkEndCommandBuffer(cmdBuffer);
  CommandStateManager::CurrentStateID = UINT32_MAX;

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
  CommandStateManager::StateToIndex.clear();
}

} // namespace Graphics