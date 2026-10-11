#include "push.hpp"
#include "Graphics/reflect.hpp"
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

PushBuffer::PushBuffer(Reflect::FlattenedReflection reflection,
                       VkShaderStageFlags stage)
    : layout(std::move(reflection)), stageFlags(stage) {

  data.resize(layout.size);
}

auto PushBuffer::GetBufferSize() const -> size_t { return layout.size; }

auto PushBuffer::GetLayout() const -> const Reflect::FlattenedReflection & {
  return layout;
}

auto PushBuffer::FlushData(VkCommandBuffer cmdBuffer) -> void {
  auto bufferSize = GetBufferSize();

  VkPushDataInfoEXT info{
      .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
      .pNext = nullptr,
      .offset = 0,
      .data =
          {
              .address = data.data(),
              .size = data.size(),
          },
  };

  vkCmdPushDataEXT(cmdBuffer, &info);
}

auto PushBuffer::ContainsUniform(const ResourceKey &key) const -> bool {
  return layout.keyToInfo.contains(key);
}

auto PushBuffer::GetUniformOffset(const ResourceKey &key) const
    -> std::optional<uint32_t> {
  auto iter = layout.keyToInfo.find(key);
  if (iter == layout.keyToInfo.end()) {
    return std::nullopt;
  }

  return iter->second.offset;
}

auto PushBuffer::GetUniform(const ResourceKey &key) const
    -> const Reflect::ResourceInfo * {
  auto iter = layout.keyToInfo.find(key);
  if (iter == layout.keyToInfo.end()) {
    return nullptr;
  }

  return &iter->second;
}

auto PushBuffer::GetStageFlags() const -> VkShaderStageFlags {
  return stageFlags;
}

auto PushBuffer::SetData(const ResourceKey &key,
                         const std::span<const uint8_t> &values) -> Error {

  auto offsetOpt = GetUniformOffset(key);
  if (!offsetOpt.has_value()) {
    return Error::Create("Uniform `" + Reflect::ResourceKeyToString(key) +
                         "` not found in push buffer.");
  }

  auto offset = offsetOpt.value();

  if (offset + values.size() > data.size()) {
    return Error::Create("Data exceeds buffer size");
  }

  // NOLINTNEXTLINE
  std::memcpy(data.data() + offset, values.data(), values.size());

  return Error::Success();
}

auto PushBuffer::SetData(const std::span<const uint8_t> &values) -> Error {
  std::memcpy(data.data(), values.data(), values.size());

  return {};
}

auto PushBuffer::SetData(const std::span<const char> &values) -> Error {
  std::memcpy(data.data(), values.data(), values.size());

  return {};
}

auto PushBuffer::GetData() -> std::span<const uint8_t> { return data; }

} // namespace Graphics