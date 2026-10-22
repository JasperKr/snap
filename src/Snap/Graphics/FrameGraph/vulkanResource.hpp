#pragma once

#include "Graphics/texture.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace Graphics {

// NOLINTBEGIN(hicpp-explicit-conversions)

struct ImageSubresource {
  VkImage image;

  uint16_t layerStart;
  uint16_t layerCount;
  uint16_t mipStart;
  uint16_t mipCount;

  ImageSubresource() = default;
  ImageSubresource(const Ref<Texture> &texture);
  ImageSubresource(const Texture *texture);
  ImageSubresource(VkImage image, uint16_t layerStart, uint16_t layerCount,
                   uint16_t mipStart, uint16_t mipCount)
      : image(image), layerCount(layerCount), layerStart(layerStart),
        mipCount(mipCount), mipStart(mipStart) {}
  ImageSubresource(VkImage image, VkImageSubresourceRange range)
      : image(image), layerCount(range.layerCount),
        layerStart(range.baseArrayLayer), mipCount(range.levelCount),
        mipStart(range.baseMipLevel) {}

  [[nodiscard]] auto Overlaps(const ImageSubresource &other) const -> bool {
    assert(mipCount != 0);
    assert(other.mipCount != 0);
    return image == other.image &&
           layerStart < (other.layerStart + other.layerCount) &&
           other.layerStart < (layerStart + layerCount) &&
           mipStart < (other.mipStart + other.mipCount) &&
           other.mipStart < (mipStart + mipCount);
  }
};

struct VulkanResource {
  enum class ResourceType : uint8_t {
    Image,
    Buffer,
    AccelerationStructure,
  };

  ResourceType type;

  union {
    ImageSubresource image;
    VkBuffer buffer;
    VkAccelerationStructureKHR accelerationStructure;
  };

  VulkanResource() = default;

  VulkanResource(const ImageSubresource &image)
      : type(ResourceType::Image), image(image) {}

  VulkanResource(VkBuffer buffer)
      : type(ResourceType::Buffer), buffer(buffer) {}

  VulkanResource(VkAccelerationStructureKHR accelerationStructure)
      : type(ResourceType::AccelerationStructure),
        accelerationStructure(accelerationStructure) {}

  [[nodiscard]] auto Hash() const -> uint64_t {
    Hash::Hasher hasher{};

    hasher.Add((uint32_t)type);

    if (type == ResourceType::AccelerationStructure) {
      hasher.Add(accelerationStructure);
      return hasher.Get();
    }

    if (type == ResourceType::Buffer) {
      hasher.Add(buffer);
      return hasher.Get();
    }

    hasher.Add(image.image);
    hasher.Add(image.layerStart);
    hasher.Add(image.layerCount);
    hasher.Add(image.mipStart);
    hasher.Add(image.mipCount);

    return hasher.Get();
  }

  [[nodiscard]]
  auto Overlaps(const VulkanResource &other) const -> bool {
    if (type != other.type) {
      return false;
    }

    switch (type) {
    case ResourceType::Image:
      return image.Overlaps(other.image);
    case ResourceType::Buffer:
      return buffer == other.buffer;
    case ResourceType::AccelerationStructure:
      return accelerationStructure == other.accelerationStructure;
    default:
      assert(false && "Unreachable");
    }
  }

  auto operator<(const VulkanResource &other) const -> bool {
    if (type != other.type) {
      return type < other.type;
    }

    switch (type) {
    case ResourceType::Image:
      return std::tie(image.image, image.layerStart, image.layerCount,
                      image.mipStart, image.mipCount) <
             std::tie(other.image.image, other.image.layerStart,
                      other.image.layerCount, other.image.mipStart,
                      other.image.mipCount);
    case ResourceType::Buffer:
      return buffer < other.buffer;
    case ResourceType::AccelerationStructure:
      return accelerationStructure < other.accelerationStructure;
    }
  }

  [[nodiscard]] auto ToString() const -> std::string {
    switch (type) {
    case ResourceType::AccelerationStructure:
      return std::format("Acceleration structure {}",
                         (void *)accelerationStructure);
    case ResourceType::Buffer:
      return std::format("Buffer {}", (void *)buffer);
    case ResourceType::Image:
      return std::format("Image {}", (void *)image.image);
    }
  }

  auto operator==(const VulkanResource &other) const noexcept -> bool {
    if (type != other.type) {
      return false;
    }

    switch (type) {
    case ResourceType::AccelerationStructure:
      return accelerationStructure == other.accelerationStructure;
    case ResourceType::Buffer:
      return buffer == other.buffer;
    case ResourceType::Image:
      return image.image == other.image.image &&
             image.layerStart == other.image.layerStart &&
             image.layerCount == other.image.layerCount &&
             image.mipStart == other.image.mipStart &&
             image.mipCount == other.image.mipCount;
    }
  }

  [[nodiscard]] auto Ptr() const -> void * {
    switch (type) {
    case ResourceType::AccelerationStructure:
      return accelerationStructure;
    case ResourceType::Buffer:
      return buffer;
    case ResourceType::Image:
      return image.image;
    }
  }
};

template <bool precise> struct VulkanResourceHash {
  auto operator()(const VulkanResource &resource) const noexcept -> uint64_t {
    if constexpr (precise) {
      return resource.Hash();
    } else {
      Hash::Hasher hasher;
      hasher.Add(static_cast<uint32_t>(resource.type));

      switch (resource.type) {
      case VulkanResource::ResourceType::Image:
        hasher.Add(resource.image.image);
        break;
      case VulkanResource::ResourceType::Buffer:
        hasher.Add(resource.buffer);
        break;
      case VulkanResource::ResourceType::AccelerationStructure:
        hasher.Add(resource.accelerationStructure);
        break;
      default:
        assert(false && "Unreachable");
      }

      return hasher.Get();
    }
  }
};

struct BoundResource {
  VulkanResource resource;
  VkAccessFlags2 access;
  VkPipelineStageFlags2 pipelines;

  [[nodiscard]]
  auto Overlaps(const BoundResource &other) const -> bool {
    return resource.Overlaps(other.resource);
  }

  [[nodiscard]]
  auto Overlaps(const VulkanResource &other) const -> bool {
    return resource.Overlaps(other);
  }

  auto operator==(const BoundResource &other) const -> bool {
    return resource == other.resource && access == other.access &&
           pipelines == other.pipelines;
  }
};

// NOLINTEND(hicpp-explicit-conversions)

} // namespace Graphics