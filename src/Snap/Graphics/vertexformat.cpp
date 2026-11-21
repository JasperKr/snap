#include "vertexformat.hpp"
#include "Graphics/FrameGraph/commands.hpp"

namespace Graphics {

auto VertexFormat::BindDynamicInputState(VirtualCommandBuffer *commandBuffer) const -> void {
  auto &threadContext = GetThreadContext();

  const auto &bindings = GetBindings();
  const auto &attributes = GetVkAttributes2();

  commandBuffer->SetVertexInputEXT(static_cast<uint32_t>(bindings.size()), bindings.data(), static_cast<uint32_t>(attributes.size()), attributes.data());
}

[[nodiscard]] auto VertexFormat::GetAttributes() const -> const std::vector<VertexComponent> & { return Attributes; }
[[nodiscard]] auto VertexFormat::GetAttribute(std::string_view name) const -> const VertexComponent * {
  for (const auto &attribute : Attributes) {
    if (attribute.name == name) {
      return &attribute;
    }
  }
  return nullptr;
}
auto VertexFormat::SetDivisor(uint32_t binding, uint32_t divisor) -> void {
  for (auto &bindingDesc : Bindings) {
    if (bindingDesc.binding == binding) {
      bindingDesc.divisor = divisor;
      return;
    }
  }
  PrintError("Binding {} not found in vertex format.", binding);
}
auto VertexFormat::SetInputRate(uint32_t binding, VkVertexInputRate inputRate) -> void {
  for (auto &bindingDesc : Bindings) {
    if (bindingDesc.binding == binding) {
      bindingDesc.inputRate = inputRate;
      return;
    }
  }
  PrintError("Binding {} not found in vertex format.", binding);
}
[[nodiscard]] auto VertexFormat::GetVkAttributes2() const -> const std::vector<VkVertexInputAttributeDescription2EXT> & { return VkAttributes2; }
[[nodiscard]] auto VertexFormat::GetBindings() const -> const std::vector<VkVertexInputBindingDescription2EXT> & { return Bindings; }
[[nodiscard]] auto VertexFormat::GetStride(uint32_t binding) const -> uint32_t {
  assert(binding < Bindings.size());
  for (const auto &bindingDesc : Bindings) {
    if (bindingDesc.binding == binding) {
      return bindingDesc.stride;
    }
  }
  PrintError("Binding {} not found in vertex format.", binding);
  return 0;
}
[[nodiscard]] auto VertexFormat::ToString() const -> std::string {
  std::string result = "VertexFormat:\n";
  for (const auto &attribute : Attributes) {
    auto formatStr = Format::ToString(attribute.format);
    auto offsetStr = std::to_string(attribute.offset);
    result = std::format("{}  - {}: {} - offset: {}\n", result, attribute.name, formatStr, offsetStr);
  }
  return result;
}
[[nodiscard]] auto VertexFormat::GetBindingCount() const -> size_t { return BindingIndices.size(); }
[[nodiscard]] auto VertexFormat::GetAttributeCount() const -> size_t { return Attributes.size(); }
[[nodiscard]] auto VertexFormat::GetBindingMapping() const -> const std::vector<uint32_t> & { return BindingIndices; }
auto VertexFormat::operator==(const VertexFormat &other) const -> bool {
  if (GetHash() != other.GetHash()) {
    return false;
  }

  if (Bindings.size() != other.Bindings.size()) {
    return false;
  }

  for (size_t i = 0; i < Bindings.size(); ++i) {
    const auto &first = Bindings.at(i);
    const auto &second = other.Bindings.at(i);

    if (first.binding != second.binding || first.stride != second.stride || first.inputRate != second.inputRate || first.divisor != second.divisor) {
      return false;
    }
  }

  if (Attributes.size() != other.Attributes.size()) {
    return false;
  }

  for (size_t i = 0; i < Attributes.size(); ++i) {
    const auto &first = Attributes.at(i);
    const auto &second = other.Attributes.at(i);

    if (first.location != second.location || first.binding != second.binding || first.format != second.format || first.offset != second.offset ||
        first.name != second.name) {
      return false;
    }
  }

  return true;
}
auto VertexFormat::operator!=(const VertexFormat &other) const -> bool { return !(*this == other); }
[[nodiscard]] auto VertexFormat::CalculateHash() const -> size_t {
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
void VertexFormat::ConstructBindings() {
  std::ranges::sort(Attributes, [](const VertexComponent &first, const VertexComponent &second) -> bool {
    if (first.binding != second.binding) {
      return first.binding < second.binding;
    }
    return first.location < second.location;
  });

  for (auto &component : Attributes) {
    assert(component.binding < MAX_BOUND_VERTEX_BUFFERS && "Vertex attribute binding exceeds maximum");
  }

  VkAttributes2.reserve(Attributes.size());
  BindingIndices.reserve(MAX_BOUND_VERTEX_BUFFERS);

  auto lastBinding = 0UL;
  for (const auto &component : Attributes) {
    // Attributes are sorted by binding, so we can just check the last used binding to avoid duplicates.
    if (BindingIndices.empty() || BindingIndices.back() != component.binding) {
      BindingIndices.emplace_back(component.binding);
    }

    assert(lastBinding <= component.binding && "Bindings must be processed in ascending order");

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
      PrintFatal("Vertex attribute '{}' has unsupported or unknown format: {}", component.name, Format::ToString(component.format));
    }
    assert(formatSize > 0 && "Vertex attribute has unsupported or unknown format: ");

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
} // namespace Graphics