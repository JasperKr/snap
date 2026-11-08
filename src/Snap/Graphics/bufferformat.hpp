#pragma once

#include "Graphics/format.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/error.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {

using BufferTreeComponent = std::variant<VkFormat, struct BufferFormat>;

struct BufferComponent;

enum class Standard : uint8_t {
  Std430,
  Std140,
};

struct BufferFormat {
  explicit BufferFormat(std::vector<BufferComponent> components, Standard std = Standard::Std430);
  explicit BufferFormat(const std::vector<std::pair<std::string, std::variant<const char *, BufferFormat>>> &components, Standard std = Standard::Std430);

  BufferFormat() = default;
  BufferFormat(const BufferFormat &other) = default;
  BufferFormat(BufferFormat &&other) noexcept = default;
  auto operator=(const BufferFormat &other) -> BufferFormat & = default;
  auto operator=(BufferFormat &&other) noexcept -> BufferFormat & = default;
  ~BufferFormat() = default;

private:
  auto FlattenComponentTree() const -> void;

  auto CalculateStride(Standard std) const -> size_t;
  auto Offset(size_t offset) const -> void;
  auto IsInitialized() const -> bool {
    if (Components.empty()) {
      return true;
    }

    bool invalid = stride == 0 || alignment == 0;
    return !invalid;
  }

  [[nodiscard]] auto FindComponent(Graphics::ResourceKey::const_iterator iterator, Graphics::ResourceKey::const_iterator end, uint64_t &arrayOffset) const
      -> BufferComponent const *;

public:
  [[nodiscard]] auto GetVkComponents() const -> const std::vector<VkFormat> & {
    if (FlatComponents.empty()) {
      FlattenComponentTree();
    }

    return FlatComponents;
  }

  [[nodiscard]] auto GetComponents() const -> const std::vector<BufferComponent> & { return Components; }

  // Get the calculated stride of the format
  [[nodiscard]] auto GetStride() const -> size_t {
    if (!IsInitialized()) {
      CalculateStride(std);
    }

    return stride;
  }

  auto operator==(const BufferFormat &other) const -> bool;

  auto operator!=(const BufferFormat &other) const -> bool { return !(*this == other); }

  [[nodiscard]] auto GetHash() const -> size_t;

  // Query the format at a given component offset
  // Useful for writing one component at a time
  // for example:
  // vec4, uvec4, float, uint, uint8[2] will give:
  // vec4, vec4, vec4, vec4, uvec4, uvec4, uvec4, uvec4, float, uint, uint8, uint8
  [[nodiscard]] auto FormatAt(size_t componentOffset) -> VkFormat;

  [[nodiscard]] auto ToString(int indentation = 0) const -> std::string;

  struct PaddingResult {
    bool needsPadding;

    // The name of the component that was misaligned, if any
    std::string needsPaddingAt;
    size_t amountOfPadding;
  };

  [[nodiscard]] auto NeedsPadding(Standard std) const -> PaddingResult;

private:
  // Definition tree
  std::vector<BufferComponent> Components;
  Standard std{};

  // The list of components in the format, inorder flattended structure tree
  mutable std::vector<VkFormat> FlatComponents;

  // The stride of the element, including any padding between components
  mutable size_t stride = 0;
  mutable uint32_t alignment{};
};

struct BufferComponent {
  std::string name;
  mutable uint32_t offset; // Offset is mutable because it is calculated after the fact
  BufferTreeComponent format;
  size_t arraySize = 1;
  bool isMatrix;

  // Vertex format only
  uint32_t location;

  [[nodiscard]] auto InternalOffsetAt(size_t index) const -> size_t {
    if (index >= arraySize || isMatrix) {
      return 0;
    }

    if (std::holds_alternative<VkFormat>(format)) {
      auto vulkanFormat = std::get<VkFormat>(format);
      return Graphics::Format::GetSize(vulkanFormat) * index;
    }

    auto bufferFormat = std::get<BufferFormat>(format);
    return bufferFormat.GetStride() * index;
  }
};

struct BufferFormatHash {
  auto operator()(const BufferFormat &format) const noexcept -> size_t { return format.GetHash(); }
};

// Get the size of a format
auto GetAlignment(VkFormat format) -> Result<size_t>;

// Get the size of a buffer component according to the given standard
auto GetAlignment(const BufferComponent &component) -> Result<size_t>;

} // namespace Graphics