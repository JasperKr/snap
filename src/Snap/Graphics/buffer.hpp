#pragma once

#include "Graphics/graphicsState.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <atomic>
#include <bit>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vulkan/vulkan_core.h"
namespace Graphics {

struct BufferCreationInfo {
  VkDeviceSize size{};
  VkBufferUsageFlags usage{};
  VkMemoryPropertyFlags properties{};

  // Staging buffers are assumed to be used only once and for large uploads
  bool stagingBuffer = false;

  // Persistent mapping keeps the buffer mapped to cpu memory for its entire lifetime
  // Useful for dynamic buffers that are updated frequently, like UBO's
  bool persistentMapping = false;

  // Debug name for the buffer
  std::string debugName;

  VkDeviceSize minAlignment{};
};

const static Type LuaBufferReadbackType = Type("BufferReadback");

struct BufferReadback : Object {
  static auto GetType() -> Type const * { return &LuaBufferReadbackType; }
  auto GetInstanceType() const -> Type const * override { return BufferReadback::GetType(); }

  Ref<struct Buffer> buffer;
  Ref<Data::ByteData> data;
  bool completed = false;
  Error error = Error::Success();

  std::mutex mutex;
  std::condition_variable conditionVar;
};

auto FlushBufferUploads(const struct GraphicsContext &context) -> Error;

static const Type LuaInternalBufferType = Type("InternalBuffer");

struct Buffer : Object, Identifiable {
  Buffer() = default;
  Buffer(const Buffer &) = delete;
  auto operator=(const Buffer &) -> Buffer & = delete;

  Buffer(Buffer &&) noexcept = delete;
  auto operator=(Buffer &&) noexcept -> Buffer & = delete;

  std::mutex mutex;

  VkBuffer handle = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  mutable void *mappedData = nullptr;

  mutable uint64_t lastUsedTimestamp{};

  VkMemoryPropertyFlags properties = 0;
  VkBufferUsageFlags usage = 0;

  static std::atomic<VkDeviceSize> TotalAllocatedMemory;

  // Indicates if this is a staging buffer, meaning it is used for temporary uploads
  bool isStagingBuffer = false;

  // Indicates if this buffer is persistently mapped, can be used to optimize frequent updates
  bool persistentMapping = false;

  // Safety flag to prevent double releases
  bool released = false;

  // Set by cleanup function otherwise we'll try to snap_defer destruction again
  bool isDestroyed = false;

  struct Address {
    VkDescriptorType type;
    uint32_t offset;
    uint32_t size;
    auto operator<(const Address &second) const -> bool { return offset < second.offset; }
    static auto Less(const Address &first, const Address &second) -> bool { return first.offset < second.offset; }

    auto operator==(const Address &other) const -> bool { return type == other.type && offset == other.offset && size == other.size; }

    [[nodiscard]] auto GetHash() const -> uint64_t {
      struct AddrHash {
        uint32_t offset;
        uint32_t size : 24;
        uint8_t type;
      };

      static const std::unordered_map<VkDescriptorType, uint8_t> typeMap{
          {VK_DESCRIPTOR_TYPE_SAMPLER, 0},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
          {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2},
          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
          {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 4},
          {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 5},
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7},
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 8},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 9},
          {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10},
          {VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 11},
          {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 12},
          {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 13},
          {VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM, 14},
          {VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM, 15},
          {VK_DESCRIPTOR_TYPE_TENSOR_ARM, 16},
          {VK_DESCRIPTOR_TYPE_MUTABLE_EXT, 17},
          {VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV, 18},
          {VK_DESCRIPTOR_TYPE_MAX_ENUM, 21},
      };

      auto addrHash = AddrHash{
          .offset = offset,
          .size = size,
          .type = typeMap.at(type),
      };

      return std::bit_cast<uint64_t>(addrHash);
    }
  };

  struct AddressHash {
    static auto operator()(const Address &addr) -> uint64_t { return addr.GetHash(); }
  };

  mutable std::unordered_map<Address, VkDeviceSize, AddressHash> addresses;
  [[nodiscard]] auto GetAddress(const GraphicsContext &context, VkDescriptorType type, uint32_t offset = 0, uint32_t size = UINT32_MAX) const
      -> Result<VkDeviceSize>;

  std::string debugName;

  auto GetTimestamp() const -> uint64_t { return lastUsedTimestamp; }
  auto MarkUse() const -> void;

  static auto Create(const Graphics::GraphicsContext &context, const Graphics::BufferCreationInfo &info) -> Result<Ref<Graphics::Buffer>>;

  ~Buffer() override;

  // Set data into the buffer at the given offset
  auto SetData(const GraphicsContext &context, const std::span<const uint8_t> &data, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) -> Error;

  template <typename T> auto SetData(const GraphicsContext &context, const std::span<T> &data, VkDeviceSize offset = 0) -> Error {
    return SetData(context,
                   std::span<const uint8_t>( // NOLINTNEXTLINE
                       reinterpret_cast<const uint8_t *>(data.data()), data.size_bytes()),
                   offset, data.size_bytes());
  }

  auto CopyTo(const GraphicsContext &context, Buffer &dstBuffer, size_t srcIndex, size_t dstIndex, size_t size) -> Error;

  auto CopyTo(const GraphicsContext &context, struct Texture &dstTexture, VkBufferImageCopy region) const -> Error;

  auto Grow(const GraphicsContext &context, size_t newSize) -> Result<Ref<Buffer>>;

  auto MapMemory(const GraphicsContext &context) -> Error;
  auto UnmapMemory(const GraphicsContext &context) -> void;

  // NOLINTNEXTLINE
  auto Clear(const GraphicsContext &context, uint32_t value, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) -> Error;

  static auto GetType() -> Type const * { return &LuaInternalBufferType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override { return Buffer::GetType(); }

  auto Upload(const GraphicsContext &context, std::span<const uint8_t> data, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) -> Error;

  auto RegisterUpload() -> void;
  auto UploadLarge(const GraphicsContext &context,
                   std::span<const uint8_t> data, // NOLINTNEXTLINE
                   VkDeviceSize offset, VkDeviceSize size) const -> Error;
  auto UploadRing(const GraphicsContext &context,
                  std::span<const uint8_t> data, // NOLINTNEXTLINE
                  VkDeviceSize offset, VkDeviceSize size) const -> Error;

  auto Readback(const GraphicsContext &context, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE,
                const Ref<Data::ByteData> &output = Ref<Data::ByteData>()) -> Result<Ref<BufferReadback>>;

  [[nodiscard]] auto GetDeviceAddress(const GraphicsContext &context) const -> Result<VkDeviceAddress>;
};

// NOLINTBEGIN
extern thread_local std::vector<Ref<Buffer>> UploadBuffers;
extern thread_local std::array<size_t, FRAMES_IN_FLIGHT> UploadBufferOffsets;
// NOLINTEND

} // namespace Graphics