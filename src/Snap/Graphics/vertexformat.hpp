#pragma once

#include "Graphics/FrameGraph/commands.hpp"
#include <cassert>
#include <cstdint>
#include <public/tracy/Tracy.hpp>
#include <string>
#include <utility>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {

struct VertexComponent {
  std::string name;
  uint32_t location{};
  uint32_t binding{};
  VkFormat format{};
  uint32_t offset{};
};

struct VertexFormat {
  VertexFormat() = default;

  explicit VertexFormat(std::vector<VertexComponent> attributes) : Attributes(std::move(attributes)) { ConstructBindings(); }

  explicit VertexFormat(std::vector<VertexComponent> attributes, std::vector<uint32_t> divisors) : Attributes(std::move(attributes)) {
    ConstructBindings();

    for (size_t i = 0; i < divisors.size(); ++i) {
      SetDivisor(static_cast<uint32_t>(i), divisors[i]);
    }

    Hash = CalculateHash();
  }

  VertexFormat(VertexFormat &&other) noexcept = default;
  auto operator=(VertexFormat &&other) noexcept -> VertexFormat & = default;
  ~VertexFormat() = default;

public:
  // Trivial copy, but not very cheap. So explicitly use Copy when needed.
  [[nodiscard]] auto Copy() const -> VertexFormat { return *this; }
  [[nodiscard]] auto GetAttributes() const -> const std::vector<VertexComponent> &;
  [[nodiscard]] auto GetAttribute(std::string_view name) const -> const VertexComponent *;

  auto SetDivisor(uint32_t binding, uint32_t divisor) -> void;
  auto SetInputRate(uint32_t binding, VkVertexInputRate inputRate) -> void;

  [[nodiscard]] auto GetVkAttributes2() const -> const std::vector<VkVertexInputAttributeDescription2EXT> &;
  [[nodiscard]] auto GetBindings() const -> const std::vector<VkVertexInputBindingDescription2EXT> &;
  [[nodiscard]] auto GetStride(uint32_t binding) const -> uint32_t;

  [[nodiscard]] auto ToString() const -> std::string;

  auto BindDynamicInputState(VirtualCommandBuffer *commandBuffer) const -> void;

  [[nodiscard]] auto GetBindingCount() const -> size_t;
  [[nodiscard]] auto GetAttributeCount() const -> size_t;

  // Returns an array mapping, for example,
  // [0, 1, 2] -> [0, 2, 5] if the vertex format has 3 bindings at indices 0, 2, and 5.
  [[nodiscard]] auto GetBindingMapping() const -> const std::vector<uint32_t> &;

  auto operator==(const VertexFormat &other) const -> bool;

  auto operator!=(const VertexFormat &other) const -> bool;

  [[nodiscard]] auto GetHash() const -> size_t { return Hash; }

private:
  [[nodiscard]] auto CalculateHash() const -> size_t;

  // Private copy
  VertexFormat(const VertexFormat &other) = default;
  auto operator=(const VertexFormat &other) -> VertexFormat & = default;

  bool constructedBindings = false;
  std::vector<VertexComponent> Attributes;

  void ConstructBindings();

  std::vector<VkVertexInputBindingDescription2EXT> Bindings;
  std::vector<VkVertexInputAttributeDescription2EXT> VkAttributes2;
  size_t Hash = 0;

  // Mapping of [0 - binding count] -> binding index
  std::vector<uint32_t> BindingIndices;
};

struct VertexFormatHash {
  auto operator()(const VertexFormat &format) const noexcept -> size_t { return format.GetHash(); }
};

} // namespace Graphics