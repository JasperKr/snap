#pragma once

#include "Graphics/buffer.hpp"
#include "Modules/Helpers/units.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr size_t InitialUniformBufferSize = Units::Memory::KiBToBytes(64);
constexpr size_t MaximumUniformBufferSize = Units::Memory::MiBToBytes(64);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::atomic<int> UniformBufferObjectCount;

namespace Graphics {

struct FrameUniformBufferObject {
public:
  static auto Create(GraphicsContext &context)
      -> Result<FrameUniformBufferObject>;

  auto Write(const std::span<const uint8_t> &data) -> Error;

  [[nodiscard]] auto GetOffset() const -> size_t { return offset; }
  [[nodiscard]] auto GetSize() const -> size_t { return buffer->size; }
  [[nodiscard]] auto GetBuffer() const -> Ref<Graphics::Buffer> {
    return buffer;
  }

  auto Finalize(const GraphicsContext &context) -> Error;
  auto NewFrame() -> void { offset = 0; }

private:
  Ref<Graphics::Buffer> buffer;
  std::vector<uint8_t> stagingBuffer =
      std::vector<uint8_t>(InitialUniformBufferSize);

  size_t internalSize = InitialUniformBufferSize;
  size_t offset{};

  size_t minUniformBufferOffsetAlignment;
};

extern thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers; // NOLINT
auto InitializeUniformBufferModule(GraphicsContext &context) -> Error;
auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject &;
auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void;

} // namespace Graphics