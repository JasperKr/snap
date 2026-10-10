#include "bufferformat.hpp"
#include "Graphics/format.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics {
BufferFormat::BufferFormat(std::vector<BufferComponent> components,
                           Standard std)
    : Components(std::move(components)), std(std) {}

BufferFormat::BufferFormat(
    const std::vector<
        std::pair<std::string, std::variant<const char *, BufferFormat>>>
        &components,
    Standard std)
    : std(std) {

  std::vector<BufferComponent> bufferComponents;
  for (const auto &[name, format] : components) {
    if (std::holds_alternative<const char *>(format)) {
      const auto *formatStr = std::get<const char *>(format);
      auto vulkanFormat = Format::FromString(formatStr);
      auto arraySize = Format::StringToArraySize(formatStr);
      assert(vulkanFormat != VK_FORMAT_UNDEFINED);

      bufferComponents.emplace_back(BufferComponent{
          .name = name,
          .format = vulkanFormat,
          .arraySize = arraySize,
          .isMatrix = arraySize > 1,
      });
    } else if (std::holds_alternative<BufferFormat>(format)) {
      const auto &nestedFormat = std::get<BufferFormat>(format);
      bufferComponents.emplace_back(BufferComponent{
          .name = name,
          .format = nestedFormat,
          .arraySize = 1,
          .isMatrix = false,
      });
    } else {
      PrintWarning("Invalid format for component {}: {}", name,
                   std::holds_alternative<const char *>(format)
                       ? std::get<const char *>(format)
                       : "nested BufferFormat");
    }
  }

  Components = std::move(bufferComponents);
  FlattenComponentTree();
}

auto BufferFormat::FindComponent(ResourceKey::const_iterator iterator,
                                 ResourceKey::const_iterator end,
                                 uint64_t &arrayOffset) const
    -> BufferComponent const * {
  if (iterator == end) {
    return nullptr;
  }

  const auto &key = *iterator;

  for (const auto &Component : Components) {
    if (key.Matches(Component.name)) {
      // End of key
      if (std::next(iterator) == end) {
        return &Component;
      }

      // Array in Struct
      if (Component.arraySize > 1 && !Component.isMatrix) {
        auto indexOpt = key.GetIndex();
        if (!indexOpt.has_value()) {
          return nullptr;
        }

        size_t index = indexOpt.value();
        if (index >= Component.arraySize) {
          return nullptr;
        }

        arrayOffset += Component.InternalOffsetAt(index);

        if (std::holds_alternative<BufferFormat>(Component.format)) {
          const auto &format = std::get<BufferFormat>(Component.format);
          return format.FindComponent(std::next(iterator), end, arrayOffset);
        }

        return &Component;
      }
    }
  }

  return nullptr;
}

auto BufferFormat::operator==(const BufferFormat &other) const -> bool {
  if (Components.size() != other.Components.size()) {
    return false;
  }

  for (size_t i = 0; i < Components.size(); ++i) {
    if (Components[i].format != other.Components[i].format ||
        Components[i].name != other.Components[i].name) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] auto BufferFormat::GetHash() const -> size_t {
  Hash::Hasher hasher{};
  for (const auto &attribute : Components) {

    // loop over Resource Key linked list

    auto begin = attribute.name.begin();
    auto end = attribute.name.end();

    for (auto it = begin; it != end; ++it) {
      hasher.Add(std::hash<std::string>()(&*it));
    }

    // hasher.Add(std::hash<uint32_t>()(attribute.format));
    if (std::holds_alternative<VkFormat>(attribute.format)) {
      hasher.Add(std::hash<uint32_t>()(std::get<VkFormat>(attribute.format)));
    } else {
      hasher.Add(std::get<BufferFormat>(attribute.format).GetHash());
    }
  }
  return hasher.Get();
}

auto GetAlignment(VkFormat format) -> Result<size_t> {
  switch (format) {
  // 32-bit vec3/vec4: alignment = 4N = 16
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_UINT:
  case VK_FORMAT_R32G32B32A32_SINT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32_SINT:
    return 16; // NOLINT
  // 32-bit vec2: alignment = 2N = 8
  // 16-bit vec3/vec4: alignment = 4N = 8
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32_SINT:

  case VK_FORMAT_R16G16B16A16_SFLOAT:
  case VK_FORMAT_R16G16B16A16_UNORM:
  case VK_FORMAT_R16G16B16A16_SNORM:
  case VK_FORMAT_R16G16B16A16_UINT:
  case VK_FORMAT_R16G16B16A16_SINT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16_SINT:
    return 8; // NOLINT
  // 32-bit scalar: alignment = N = 4
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32_SINT:
  // 16-bit vec2: alignment = 2N = 4
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16_SINT:
  // 8-bit vec3/vec4: alignment = 4N = 4
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_R8G8B8_SINT:
    return 4; // NOLINT
  // 16-bit scalar: alignment = N = 2
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16_SINT:
  // 8-bit vec2: alignment = 2N = 2
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8_SINT:
    return 2; // NOLINT
  // 8-bit scalar: alignment = N = 1
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8_SINT:
    return 1; // NOLINT
  default:
    return Error::Unexpected("Unsupported format for alignment: " +
                             std::to_string(format));
  }
}

inline auto AlignUp(size_t offset, size_t alignment) -> size_t {
  return (offset + alignment - 1) & ~(alignment - 1);
}

auto BufferFormat::Offset(size_t offset) const -> void {
  for (const auto &component : Components) {
    component.offset += offset;

    if (std::holds_alternative<BufferFormat>(component.format)) {
      const auto &format = std::get<BufferFormat>(component.format);
      format.Offset(offset);
    }
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto BufferFormat::CalculateStride(Standard std) const -> size_t {
  if (Components.empty()) {
    PrintWarning("Buffer format has no components, stride will be 0");
  }

  size_t offset = 0;

  size_t maxAlignment = 0;
  for (const auto &format : Components) {
    size_t baseAlign = 0;

    // Get base alignment of component
    if (std::holds_alternative<VkFormat>(format.format)) {
      auto vulkanFormat = std::get<VkFormat>(format.format);
      VkFormat alignFormat =
          format.arraySize == 1
              ? vulkanFormat
              : Graphics::Format::GetVec4Variant(vulkanFormat);
      auto alignResult = GetAlignment(alignFormat);
      if (Error::IsError(alignResult)) {
        return 0;
      }

      baseAlign = alignResult.value();
    } else if (std::holds_alternative<BufferFormat>(format.format)) {
      auto bufferFormat = std::get<BufferFormat>(format.format);
      bufferFormat.CalculateStride(std);

      baseAlign = bufferFormat.alignment;
    }

    size_t compAlignment = 0;

    // Get allowed alignment based on standard
    switch (std) {
    case Standard::Std140:
      compAlignment = (format.arraySize == 1)
                          ? baseAlign
                          : std::max(baseAlign, 16UL); // NOLINT
      break;
    case Standard::Std430:
      compAlignment = baseAlign;
      break;
    }
    maxAlignment = std::max(maxAlignment, compAlignment);
    size_t size = 0;

    // Get size of component
    if (std::holds_alternative<VkFormat>(format.format)) {
      auto vulkanFormat = std::get<VkFormat>(format.format);
      size = Graphics::Format::GetSize(vulkanFormat);
    } else if (std::holds_alternative<BufferFormat>(format.format)) {
      auto bufferFormat = std::get<BufferFormat>(format.format);
      size = bufferFormat.CalculateStride(std);
    }

    // Align component
    offset = AlignUp(offset, compAlignment);
    format.offset = offset;

    if (std::holds_alternative<BufferFormat>(format.format)) {
      const auto &bufferFormat = std::get<BufferFormat>(format.format);
      bufferFormat.Offset(offset);
    }

    // Add padding
    if (format.arraySize == 1) {
      offset += size;
    } else {
      switch (std) {
      case Standard::Std140:
        offset += AlignUp(size, 16) * format.arraySize; // NOLINT
        break;
      case Standard::Std430:
        offset += AlignUp(size, compAlignment) * format.arraySize;
        break;
      }
    }
  }

  // Pad final structure
  switch (std) {
  case Standard::Std140: {
    size_t finalAlignment = std::max(maxAlignment, 16UL); // NOLINT
    alignment = finalAlignment;
    stride = AlignUp(offset, finalAlignment);
    break;
  }
  case Standard::Std430: {
    alignment = maxAlignment;
    stride = AlignUp(offset, maxAlignment);
    break;
  }
  }

  return stride;
}

auto BufferFormat::FlattenComponentTree() const -> void {
  for (const auto &component : Components) {
    if (std::holds_alternative<VkFormat>(component.format)) {
      const auto &bufferComp = std::get<VkFormat>(component.format);
      // Copy this component for each array element
      for (int i = 0; i < component.arraySize; i++) {
        FlatComponents.emplace_back(bufferComp);
      }
    } else if (std::holds_alternative<BufferFormat>(component.format)) {
      const auto &format = std::get<BufferFormat>(component.format);

      if (format.FlatComponents.empty()) {
        format.FlattenComponentTree();
      }

      for (auto &comp : format.FlatComponents) {
        FlatComponents.emplace_back(comp);
      }
    }
  }
}

[[nodiscard]] auto BufferFormat::FormatAt(size_t componentOffset) -> VkFormat {
  if (FlatComponents.empty()) {
    FlattenComponentTree();
  }

  componentOffset %= FlatComponents.size();

  return FlatComponents[componentOffset];
}

[[nodiscard]] auto BufferFormat::ToString(int indentation) const
    -> std::string {
  auto baseTabs = std::string(indentation * 2UL, ' ');
  std::ostringstream output;
  output << baseTabs << "{\n";
  indentation++;

  for (const auto &component : Components) {
    auto tabs = std::string(indentation * 2UL, ' ');
    auto arrayStr = component.arraySize == 1
                        ? ""
                        : std::format("[{}]", component.arraySize);
    if (std::holds_alternative<VkFormat>(component.format)) {
      auto format = std::get<VkFormat>(component.format);
      auto formatname = Graphics::Format::ToString(format);
      output << std::format("{}{}: {}{};\n", tabs, component.name, formatname,
                            arrayStr);
    } else {
      auto format = std::get<Graphics::BufferFormat>(component.format);
      output << std::format("{}{}\n{}{}", tabs, component.name,
                            format.ToString(indentation), arrayStr);
    }
  }

  output << baseTabs << "};\n";

  return output.str();
}

[[nodiscard]] auto BufferFormat::NeedsPadding(Standard std) const
    -> PaddingResult {
  size_t offset = 0;
  for (const auto &component : Components) {
    if (component.offset != offset) {
      return {
          .needsPadding = true,
          .needsPaddingAt = component.name,
          .amountOfPadding = component.offset - offset,
      };
    }

    size_t size = 0;
    if (std::holds_alternative<VkFormat>(component.format)) {
      auto vulkanFormat = std::get<VkFormat>(component.format);
      size = Graphics::Format::GetSize(vulkanFormat);
    } else if (std::holds_alternative<BufferFormat>(component.format)) {
      auto bufferFormat = std::get<BufferFormat>(component.format);
      size = bufferFormat.GetStride();

      auto paddingResult = bufferFormat.NeedsPadding(std);
      if (paddingResult.needsPadding) {
        return paddingResult;
      }
    }

    offset += size * component.arraySize;
  }

  return {
      .needsPadding = false,
  };
}

} // namespace Graphics