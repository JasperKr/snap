#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <algorithm>
#include <iterator>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

struct MemoryRange {
  VkDeviceSize start;
  VkDeviceSize end;

  auto TryJoin(MemoryRange other) -> Result<bool> {
    // Case A: [ A  ]
    //                [  B  ]
    // Case B: [ A  ]
    //              [  B  ]
    // Case C: [ A  ]
    //            [  B  ]
    // Case D:     [ A  ]
    //         [  B  ]
    // Case E: [   A   ]
    //           [ B ]
    // Case F:   [ A ]
    //         [   B   ]

    // A
    if (end <= other.start || start >= other.end) {
      return false;
    }

    // B-1
    if (start == other.end) {
      start = other.start;
      return true;
    }

    // B-2
    if (end == other.start) {
      end = other.end;
      return true;
    }

#ifndef NDEBUG
    if (end > other.start && start < other.start) {
      return Error::Unexpected("Extent partial overlap.");
    }

    if (start < other.end && end > other.end) {
      return Error::Unexpected("Extent partial overlap.");
    }

    if (start < other.start && other.end < end) {
      return Error::Unexpected("This extent contains other extent.");
    }

    if (other.start <= start && end <= other.end) {
      return Error::Unexpected("Other contains this extent.");
    }
#endif

    return Error::Unexpected("Unexpected error joining memory ranges.");
  }

  auto operator<(VkDeviceSize address) const noexcept -> bool {
    return start < address;
  }
};

struct MemoryManager {
  // Gup Geep Gip Gap???
  std::vector<MemoryRange> memoryGapRanges;

  void Deallocate(VkDeviceSize address, VkDeviceSize size,
                  VkDeviceSize &cursor) {
    auto end = address + size;

    // Find the first element that doesn't return true with less on end
    auto iter = std::ranges::lower_bound(
        memoryGapRanges, address, std::ranges::less{}, &MemoryRange::end);

    // If the end of that last range is our address, we can combine.
    if (iter != memoryGapRanges.end() && iter->end == address) {
      iter->end = end;

      auto nextItem = std::next(iter);
      if (nextItem != memoryGapRanges.end()) {
        if (iter->TryJoin(*nextItem)) {
          memoryGapRanges.erase(nextItem);
        }
      }
    } else {
      // We couldn't append to the end of any range

      // Find the first element that doesn't return true with less on start
      iter = std::ranges::lower_bound(memoryGapRanges, address,
                                      std::ranges::less{}, &MemoryRange::start);

      // If that last range starts at our range end, then we resize it.
      if (iter != memoryGapRanges.end() && iter->start == end) {
        iter->start = address;
      } else {
        // Otherwise we insert a new range after it
        memoryGapRanges.insert(iter, MemoryRange{.start = address, .end = end});
      }
    }

    while (!memoryGapRanges.empty() && memoryGapRanges.back().end == cursor) {
      cursor = memoryGapRanges.back().start;
      memoryGapRanges.pop_back();
    }
  }

  auto TryAllocateFromGap(VkDeviceSize size)
      -> Result<std::optional<VkDeviceSize>> {
    if (memoryGapRanges.empty()) {
      return std::nullopt;
    }

    auto &range = memoryGapRanges.front();
    auto address = range.start;

    auto newStart = range.start + size;

    ERR_ASSERT_MSG(newStart <= range.end, "Out of memory");

    if (newStart == range.end) {
      memoryGapRanges.erase(memoryGapRanges.begin());
      return address;
    }

    range.start = newStart;

    return address;
  }
};

struct DescriptorHeapManager {
  Ref<Buffer> ResourceHeap;
  Ref<Buffer> SamplerHeap;

  VkDeviceAddress resourceAddress{};
  VkDeviceAddress samplerAddress{};

  MemoryManager bufferMemoryManager;
  MemoryManager imageMemoryManager;
  MemoryManager samplerMemoryManager;

  // Internal write positions
  VkDeviceSize bufferCursor{};
  VkDeviceSize imageCursor{};
  VkDeviceSize samplerCursor{};

  auto AllocateResource(const struct GraphicsContext &context,
                        const Ref<Buffer> &buffer, VkDescriptorType type)
      -> Result<VkDeviceSize>;
  auto AllocateResource(const struct GraphicsContext &context,
                        const Ref<Texture> &texture, VkDescriptorType type)
      -> Result<VkDeviceSize>;
  auto AllocateSampler(const struct GraphicsContext &context,
                       VkSamplerCreateInfo samplerCreateInfo)
      -> Result<VkDeviceSize>;

  auto FreeImageResourceIndex(VkDeviceSize resourcePtr) -> Error;
  auto FreeBufferResourceIndex(VkDeviceSize resourcePtr) -> Error;
  auto FreeSamplerResourceIndex(VkDeviceSize resourcePtr) -> Error;

  auto Initialize(const struct GraphicsContext &context) -> Error;
  auto DeInitialize() -> void;

  VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProperties{};
  VkDeviceSize BufferCursorOrigin{};

  static auto GetInstance() -> DescriptorHeapManager & {
    static DescriptorHeapManager manager;
    return manager;
  }

private:
  auto AllocateResource(bool isImage) -> Result<VkDeviceSize>;
  auto AllocateSampler() -> Result<VkDeviceSize>;
};

} // namespace Graphics