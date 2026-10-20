#include "uniform.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <vector>

#include "Modules/Helpers/utils.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "vulkan/vulkan_core.h"

namespace Graphics {

thread_local std::vector<FrameUniformBufferObject>
    ThreadUniformBuffers{}; // NOLINT

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<int> UniformBufferObjectCount{0};

auto InitializeUniformBufferModule(GraphicsContext &context) -> Error {
  for (uint32_t j = 0; j < FRAMES_IN_FLIGHT; j++) {
    auto buffer = CHECK_RES(FrameUniformBufferObject::Create(context));

    ThreadUniformBuffers.emplace_back(buffer);
  }

  return Error::Success();
}

auto DeInitializeUniformBufferModule(GraphicsContext &context) -> void {
  ThreadUniformBuffers.clear();
}

auto GetGlobalUniformBuffer(uint32_t frameIndex) -> FrameUniformBufferObject & {
  assert(frameIndex < ThreadUniformBuffers.size() &&
         "Frame index out of bounds in GetGlobalUniformBuffer");
  return ThreadUniformBuffers.at(frameIndex);
}

auto FrameUniformBufferObject::Create(GraphicsContext &context)
    -> Result<FrameUniformBufferObject> {

  FrameUniformBufferObject obj{};
  BufferCreationInfo info{};
  info.size = InitialUniformBufferSize;
  info.persistentMapping = true;
  info.stagingBuffer = true;
  info.properties = static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
                    static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
                    static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
               static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  info.debugName = "Frame Uniform Buffer";

  obj.buffer = CHECK_RES(Buffer::Create(context, info));
  obj.minUniformBufferOffsetAlignment =
      context.deviceProperties.limits.minUniformBufferOffsetAlignment;

  return obj;
}

auto FrameUniformBufferObject::Write(const std::span<const uint8_t> &data)
    -> Error {
  ZoneScoped;

  [[unlikely]]
  if (data.size() == 0) {
    return {};
  }

  [[unlikely]]
  if (data.size() > MaximumUniformBufferSize) {
    return Error::Create(
        "Tried to set uniform buffer data larger than maximum. (holy shit)");
  }

  const auto requiredSize = offset + data.size();

  [[unlikely]]
  if (requiredSize > internalSize) {
    while (requiredSize > internalSize) {
      internalSize *= 2;
    }

    [[unlikely]]
    if (internalSize > MaximumUniformBufferSize) {
      return Error::Create("Uniform buffer exceeded maximum allowed size.");
    }

    stagingBuffer.resize(internalSize);
  }

  assert(requiredSize <= stagingBuffer.size() &&
         "Write operation exceeds the size of the staging buffer.");

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  memcpy(stagingBuffer.data() + offset, data.data(), data.size());

  offset += static_cast<uint32_t>(data.size());
  offset = Utils::AlignUp(offset, minUniformBufferOffsetAlignment);

  return {};
}

auto FrameUniformBufferObject::Finalize(const GraphicsContext &context)
    -> Error {
  ZoneScoped;

  [[unlikely]]
  if (internalSize != buffer->size) {
    Graphics::BufferCreationInfo info{};
    info.size = internalSize;
    info.persistentMapping = false;
    info.stagingBuffer = true;
    info.properties =
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) |
        static_cast<uint32_t>(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    info.usage = static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                 static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    info.debugName = "Frame Uniform Buffer";

    buffer = CHECK_RES(Graphics::Buffer::Create(context, info));
  }

  CHECK_ERR(buffer->SetData(context, stagingBuffer, 0, offset));

  return {};
}

} // namespace Graphics