#include "structured.hpp"

#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "tl/expected.hpp"

namespace Graphics {

auto StructuredBuffer::Create(const GraphicsContext &context,
                              const BufferFormat &format, size_t elementCount,
                              StructuredBufferCreationInfo const &info)
    -> Result<Ref<StructuredBuffer>> {

  if (format.GetStride() == 0) {
    return Error::Unexpectedf(
        "Invalid format, Cannot create structured buffer with a stride of 0");
  }

  Graphics::BufferCreationInfo bufferCreateInfo{
      .size = format.GetStride() * elementCount,
      .usage = info.usageFlags,
      .properties = info.memoryFlags,
      .debugName = info.debugName,
  };

  auto buffer = Ref<StructuredBuffer>::Make();

  buffer->format = format;
  buffer->elementCount = elementCount;
  buffer->elementStride = format.GetStride();
  buffer->buffer = CHECK_RES(Buffer::Create(context, bufferCreateInfo));

  return buffer;
}

auto StructuredBuffer::GetFormat() -> BufferFormat & { return format; }

auto StructuredBuffer::Clear(GraphicsContext &context, uint32_t value,
                             VkDeviceSize offset, VkDeviceSize size) const
    -> Error {
  return buffer->Clear(context, value, offset, size);
}

// Copies the old buffer's data into a new buffer with the new size
auto StructuredBuffer::Grow(const GraphicsContext &context,
                            size_t newElementCount)
    -> Result<Ref<StructuredBuffer>> {
  auto newBuffer =
      CHECK_RES(buffer->Grow(context, newElementCount * elementStride));

  auto newStructuredBuffer = Ref<StructuredBuffer>::Make();
  newStructuredBuffer->format = format;
  newStructuredBuffer->elementCount = newElementCount;
  newStructuredBuffer->elementStride = elementStride;
  newStructuredBuffer->buffer = newBuffer;

  return newStructuredBuffer;
}

} // namespace Graphics