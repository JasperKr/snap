#pragma once

#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/stackVector.hpp"
#include <mutex>
#include <unordered_map>

namespace Graphics {

struct DescriptorKey {
  Math::StackVector<ResourceBinding, 16> bindings; // sorted by binding NOLINT

  auto operator==(const DescriptorKey &other) const -> bool {
    return bindings == other.bindings;
  }
};

struct DescriptorKeyHash {
  auto operator()(const DescriptorKey &key) const noexcept -> size_t {
    Hash::Hasher hasher{};

    for (const auto &binding : key.bindings) {
      hasher.Add(binding.binding);
      hasher.Add(binding.resource);
    }

    return hasher.Get();
  }
};

struct DescriptorSetLayoutKey {
  VkDescriptorSetLayoutCreateFlags flags;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkDescriptorBindingFlags> bindingFlags;

  auto operator==(const DescriptorSetLayoutKey &other) const -> bool {
    if (flags != other.flags) {
      return false;
    }

    if (bindings.size() != other.bindings.size()) {
      return false;
    }

    for (size_t i = 0; i < bindings.size(); ++i) {
      const auto &firstBinding = bindings[i];
      const auto &secondBinding = other.bindings[i];

      if (firstBinding.binding != secondBinding.binding ||
          firstBinding.descriptorType != secondBinding.descriptorType ||
          firstBinding.descriptorCount != secondBinding.descriptorCount ||
          firstBinding.stageFlags != secondBinding.stageFlags) {
        return false;
      }

      // Compare immutable samplers if they exist
      [[unlikely]]
      if (firstBinding.pImmutableSamplers != nullptr &&
          secondBinding.pImmutableSamplers != nullptr) {
        for (uint32_t j = 0; j < firstBinding.descriptorCount; ++j) {
          // NOLINTBEGIN
          if (firstBinding.pImmutableSamplers[j] !=
              secondBinding.pImmutableSamplers[j]) {
            return false;
          }
          // NOLINTEND
        }
      } else if (firstBinding.pImmutableSamplers !=
                 secondBinding.pImmutableSamplers) {
        // One is null, the other is not
        return false;
      }
    }

    if (bindingFlags.size() != other.bindingFlags.size()) {
      return false;
    }

    for (size_t i = 0; i < bindingFlags.size(); ++i) {
      if (bindingFlags[i] != other.bindingFlags[i]) {
        return false;
      }
    }

    return true;
  }
};

struct DescriptorSetLayoutKeyHash {
  auto operator()(DescriptorSetLayoutKey const &key) const noexcept -> size_t {
    Hash::Hasher hasher{};

    hasher.Add(key.flags);

    for (const auto &binding : key.bindings) {
      hasher.Add(binding.binding);
      hasher.Add(binding.descriptorType);
      hasher.Add(binding.descriptorCount);
      hasher.Add(binding.stageFlags);

      [[unlikely]]
      if (binding.pImmutableSamplers != nullptr) {
        // I do not use Immutable samplers, but it is included for completeness, so it is marked as an unlikely branch
        for (uint32_t i = 0; i < binding.descriptorCount; ++i) {
          // NOLINTNEXTLINE, reinterpret cast And pointer arithmetic
          hasher.Add(reinterpret_cast<size_t>(binding.pImmutableSamplers[i]));
        }
      }
    }

    for (VkDescriptorBindingFlags flag : key.bindingFlags) {
      hasher.Add(flag);
    }

    return hasher.Get();
  }
};

struct DescriptorCache {
  std::mutex descriptorSetLayoutCacheMutex;
  std::unordered_map<DescriptorSetLayoutKey, VkDescriptorSetLayout,
                     DescriptorSetLayoutKeyHash>
      descriptorSetLayoutCache;

  VkDescriptorSetLayout emptySetLayout = VK_NULL_HANDLE;

  // Key to cache descriptor sets based on layout and resource pointers
  // Immutable pointers but a weak reference is needed to avoid keeping resources alive indefinitely
  LRUCache<DescriptorKey, VkDescriptorSet, DescriptorKeyHash>
      descriptorSetCache{128}; // NOLINT

  auto GetDescriptorSetLayout(const DescriptorSetLayoutKey &layoutKey,
                              const GraphicsContext &context)
      -> Result<VkDescriptorSetLayout>;

  auto Initialize(const GraphicsContext &context) -> Error;
  auto DeInitialize(const GraphicsContext &context) -> void;
};

auto GetDescriptorCache() -> DescriptorCache &;

} // namespace Graphics