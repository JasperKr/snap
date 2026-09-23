#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
namespace Engine::Renderer {
struct BufferUploadManager {
  BufferUploadManager() = default;
  BufferUploadManager(const Ref<Graphics::Buffer> &buffer, size_t elementStride)
      : buffer(buffer), elementStride(elementStride) {
    assert(buffer.isValid());
    data.resize(buffer->size);
  }

  auto SetNewBuffer(const Ref<Graphics::Buffer> &newBuffer) -> Error {
    ERR_ASSERT(newBuffer->size > buffer->size);
    data.resize(newBuffer->size);
    buffer = newBuffer;

    return {};
  }

  auto MarkUpdated(uint32_t elementIndex) -> void {
    dirtyIndices.emplace_back(elementIndex);
  }

  auto Flush(const Graphics::GraphicsContext &context) -> Error {
    if (dirtyIndices.empty()) {
      return {};
    }

    ERR_ASSERT(buffer.isValid());
    ERR_ASSERT(elementStride != 0);

    auto flush = [&](Math::Uvec2 range) -> Error {
      const auto start = range.x * elementStride;
      const auto end = range.y * elementStride;

      PrintAlways("Flushing range {} - {}", start, end);

      // NOLINTNEXTLINE
      return buffer->SetData(context, {data.data() + start, data.data() + end},
                             start);
    };

    std::ranges::sort(dirtyIndices);

    auto currentRange =
        Math::Uvec2(dirtyIndices.front(), dirtyIndices.front() + 1);

    for (size_t i = 1; i < dirtyIndices.size(); ++i) {
      const auto idx = dirtyIndices[i];

      if (idx <= currentRange.y) {
        currentRange.y = std::max(currentRange.y, idx + 1);
      } else {
        CHECK_ERR(flush(currentRange));
        currentRange = Math::Uvec2(idx, idx + 1);
      }
    }

    CHECK_ERR(flush(currentRange));

    dirtyIndices.clear();
    return {};
  }

  auto GetPtrAt(uint32_t elementIndex) -> uint8_t * {
    assert(data.size() >= (elementIndex + 1) * elementStride);
    return data.data() + (elementIndex * elementStride); // NOLINT
  }

private:
  Ref<Graphics::Buffer> buffer;
  std::vector<uint8_t> data;
  size_t elementStride{};
  std::vector<uint32_t> dirtyIndices;
};
} // namespace Engine::Renderer