#pragma once

#include "Modules/Helpers/hasher.hpp"
#include "Modules/stackVector.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vulkan/vulkan_core.h>
namespace Graphics {

inline auto GetIsCurrentlyRendering() -> bool & {
  thread_local bool CurrentlyRendering = false;
  return CurrentlyRendering;
}

inline auto GetIsStateDirty() -> bool & {
  thread_local bool StateDirty = true;
  return StateDirty;
}

// Use if the state gets invalidated,
// for example, new command buffer.
inline auto SetDirtyState() -> void { GetIsStateDirty() = true; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline auto DefaultPixelFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr uint32_t MAX_COLOR_ATTACHMENTS = 8;

constexpr VkPipelineColorBlendAttachmentState DefaultBlendMode = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr VkPipelineColorBlendAttachmentState BlendmodeNone = {
    .blendEnable = VK_FALSE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr VkPipelineColorBlendAttachmentState BlendmodeAdditive = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr VkPipelineColorBlendAttachmentState BlendmodeMultiply = {
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .colorWriteMask = static_cast<uint32_t>(VK_COLOR_COMPONENT_R_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_G_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_B_BIT) |
                      static_cast<uint32_t>(VK_COLOR_COMPONENT_A_BIT),
};

constexpr uint32_t FRAMES_IN_FLIGHT = 3;
constexpr uint32_t SWAPCHAIN_IMAGE_COUNT = 3;
constexpr uint32_t MAX_SWAPCHAIN_IMAGES = 8;
constexpr uint32_t MAX_BOUND_VERTEX_BUFFERS = 8;

struct KeyElement {
  KeyElement() = default;
  KeyElement(const char *key) : Key(key) {} // NOLINT
  explicit KeyElement(std::string_view key) : Key(key.data()) {}
  explicit KeyElement(std::string_view key, uint64_t index)
      : Key(key.data()), Index(index) {}

  const char *Key = nullptr;
  uint64_t Index = UINT64_MAX;

  [[nodiscard]] auto Matches(std::string_view key) const -> bool {
    return Key == key;
  }

  [[nodiscard]] auto Matches(const char *key) const -> bool {
    return Matches(std::string_view{key});
  }

  [[nodiscard]] auto GetIndex() const -> std::optional<uint64_t> {
    return Index == UINT64_MAX ? std::nullopt : std::optional<uint64_t>{Index};
  }

  [[nodiscard]] auto IsIndexingKey() const -> bool {
    return Index != UINT64_MAX;
  }

  [[nodiscard]] auto ToString() const -> std::string {
    if (Index != UINT64_MAX) {
      return std::string(Key) + "[" + std::to_string(Index) + "]";
    }

    return Key;
  }

  auto operator==(const KeyElement &other) const -> bool {
    return std::string_view(Key) == std::string_view(other.Key) &&
           Index == other.Index;
  }
};

// While a max limit on resource depth is a bit annoying
// Allowing stack allocation of resource keys is probably worth it for performance reasons
constexpr size_t MAX_KEY_ELEMENTS = 8;

using ResourceKey = Math::StackVector<KeyElement, MAX_KEY_ELEMENTS>;

struct ResourceKeyHash {
  auto operator()(const ResourceKey &key) const -> size_t {
    Hash::Hasher hasher;
    for (const auto &element : key) {
      hasher.Add(element.Key);
      hasher.Add(element.Index);
    }

    return hasher.Get();
  }
};

} // namespace Graphics