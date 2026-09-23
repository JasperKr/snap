#pragma once

#include "Graphics/format.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/Helpers/hasher.hpp"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <public/tracy/Tracy.hpp>
#include <string>
#include <utility>
#include <vector>

#include "Modules/console.hpp"
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
  explicit VertexFormat(std::vector<VertexComponent> attributes)
      : Attributes(std::move(attributes)) {
    ConstructBindings();
  }

  explicit VertexFormat(std::vector<VertexComponent> attributes,
                        std::vector<uint32_t> divisors)
      : Attributes(std::move(attributes)) {
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

  [[nodiscard]] auto GetAttributes() const
      -> const std::vector<VertexComponent> & {
    return Attributes;
  }

  [[nodiscard]] auto GetAttribute(std::string_view name) const
      -> const VertexComponent * {
    for (const auto &attribute : Attributes) {
      if (attribute.name == name) {
        return &attribute;
      }
    }
    return nullptr;
  }

  auto SetDivisor(uint32_t binding, uint32_t divisor) -> void {
    for (auto &bindingDesc : Bindings) {
      if (bindingDesc.binding == binding) {
        bindingDesc.divisor = divisor;
        return;
      }
    }
    PrintError("Binding {} not found in vertex format.", binding);
  }

  auto SetInputRate(uint32_t binding, VkVertexInputRate inputRate) -> void {
    for (auto &bindingDesc : Bindings) {
      if (bindingDesc.binding == binding) {
        bindingDesc.inputRate = inputRate;
        return;
      }
    }
    PrintError("Binding {} not found in vertex format.", binding);
  }

  [[nodiscard]] auto GetVkAttributes2()
      -> std::vector<VkVertexInputAttributeDescription2EXT> & {
    return VkAttributes2;
  }

  [[nodiscard]] auto GetBindings() const
      -> const std::vector<VkVertexInputBindingDescription2EXT> & {
    return Bindings;
  }

  [[nodiscard]] auto GetStride(uint32_t binding) const -> uint32_t {
    assert(binding < Bindings.size());
    for (const auto &bindingDesc : Bindings) {
      if (bindingDesc.binding == binding) {
        return bindingDesc.stride;
      }
    }
    PrintError("Binding {} not found in vertex format.", binding);
    return 0;
  }

  [[nodiscard]] auto ToString() const -> std::string {
    std::string result = "VertexFormat:\n";
    for (const auto &attribute : Attributes) {
      auto formatStr = Format::ToString(attribute.format);
      auto offsetStr = std::to_string(attribute.offset);
      result = std::format("{}  - {}: {} - offset: {}\n", result,
                           attribute.name, formatStr, offsetStr);
    }
    return result;
  }

  auto BindDynamicInputState(struct VirtualCommandBuffer *commandBuffer)
      -> void;

  [[nodiscard]] auto GetBindingCount() const -> size_t {
    return BindingIndices.size();
  }
  [[nodiscard]] auto GetAttributeCount() const -> size_t {
    return Attributes.size();
  }

  // Returns an array mapping, for example,
  // [0, 1, 2] -> [0, 2, 5] if the vertex format has 3 bindings at indices 0, 2, and 5.
  [[nodiscard]] auto GetBindingMapping() const
      -> const std::vector<uint32_t> & {
    return BindingIndices;
  }

  auto operator==(const VertexFormat &other) const -> bool {
    if (GetHash() != other.GetHash()) {
      return false;
    }

    if (Attributes.size() != other.Attributes.size()) {
      return false;
    }

    for (size_t i = 0; i < Attributes.size(); ++i) {
      if (Attributes[i].location != other.Attributes[i].location ||
          Attributes[i].binding != other.Attributes[i].binding ||
          Attributes[i].format != other.Attributes[i].format ||
          Attributes[i].offset != other.Attributes[i].offset ||
          Attributes[i].name != other.Attributes[i].name) {
        return false;
      }
    }

    return true;
  }

  auto operator!=(const VertexFormat &other) const -> bool {
    return !(*this == other);
  }

  [[nodiscard]] auto GetHash() const -> size_t { return Hash; }

private:
  [[nodiscard]] auto CalculateHash() const -> size_t {
    Hash::Hasher hasher{};
    for (const auto &attribute : Attributes) {
      hasher.Add(std::hash<std::string>()(attribute.name));
      hasher.Add(std::hash<uint32_t>()(attribute.location));
      hasher.Add(std::hash<uint32_t>()(attribute.binding));
      hasher.Add(std::hash<uint32_t>()(attribute.format));
      hasher.Add(std::hash<uint32_t>()(attribute.offset));
    }
    return hasher.Get();
  }

  // Private copy
  VertexFormat(const VertexFormat &other) = default;
  auto operator=(const VertexFormat &other) -> VertexFormat & = default;

  bool constructedBindings = false;
  std::vector<VertexComponent> Attributes;

  // NOLINTNEXTLINE
  void ConstructBindings() {
    std::ranges::sort(Attributes,
                      [](const VertexComponent &first,
                         const VertexComponent &second) -> bool {
                        if (first.binding != second.binding) {
                          return first.binding < second.binding;
                        }
                        return first.location < second.location;
                      });

    for (auto &component : Attributes) {
      assert(component.binding < MAX_BOUND_VERTEX_BUFFERS &&
             "Vertex attribute binding exceeds maximum");
    }

    VkAttributes2.reserve(Attributes.size());
    BindingIndices.reserve(MAX_BOUND_VERTEX_BUFFERS);

    auto lastBinding = 0UL;
    for (const auto &component : Attributes) {
      // Attributes are sorted by binding, so we can just check the last used binding to avoid duplicates.
      if (BindingIndices.empty() ||
          BindingIndices.back() != component.binding) {
        BindingIndices.emplace_back(component.binding);
      }

      assert(lastBinding <= component.binding &&
             "Bindings must be processed in ascending order");

      lastBinding = component.binding;
    }

    Bindings.reserve(BindingIndices.size());
    std::vector<uint32_t> bindingToBindingIndex{};
    bindingToBindingIndex.resize(MAX_BOUND_VERTEX_BUFFERS);

    for (auto binding : BindingIndices) {
      bindingToBindingIndex.at(binding) = Bindings.size();

      Bindings.emplace_back(VkVertexInputBindingDescription2EXT{
          .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
          .pNext = nullptr,
          .binding = binding,
          .stride = 0,
          .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
          .divisor = 1,
      });
    }

    for (auto &component : Attributes) {
      auto formatSize = Format::GetSize(component.format);

      if (formatSize == 0) {
        PrintFatal(
            "Vertex attribute '{}' has unsupported or unknown format: {}",
            component.name, Format::ToString(component.format));
      }
      assert(formatSize > 0 &&
             "Vertex attribute has unsupported or unknown format: ");

      auto idx = bindingToBindingIndex.at(component.binding);

      component.offset = Bindings[idx].stride;
      Bindings[idx].stride += formatSize;

      VkAttributes2.emplace_back(VkVertexInputAttributeDescription2EXT{
          .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
          .location = component.location,
          .binding = component.binding,
          .format = component.format,
          .offset = component.offset,
      });
    }

    constructedBindings = true;
    Hash = CalculateHash();
  }

  std::vector<VkVertexInputBindingDescription2EXT> Bindings;
  std::vector<VkVertexInputAttributeDescription2EXT> VkAttributes2;
  size_t Hash = 0;

  // Mapping of [0 - binding count] -> binding index
  std::vector<uint32_t> BindingIndices;
};

struct VertexFormatHash {
  auto operator()(const VertexFormat &format) const noexcept -> size_t {
    return format.GetHash();
  }
};

} // namespace Graphics