#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Graphics/graphicsState.hpp"
#include "Graphics/reflect.hpp"
#include "Modules/error.hpp"

namespace Graphics {

struct PushBuffer {
  explicit PushBuffer(Reflect::FlattenedReflection reflection,
                      VkShaderStageFlags stage = VK_SHADER_STAGE_ALL);

  [[nodiscard]] auto GetBufferOffset() const -> size_t;
  [[nodiscard]] auto GetBufferSize() const -> size_t;
  [[nodiscard]] auto GetLayout() const -> const Reflect::FlattenedReflection &;
  auto FlushData(VkPipelineLayout layout, VkCommandBuffer cmdBuffer) -> void;

  auto SetData(const ResourceKey &key, const std::span<const uint8_t> &values)
      -> Error;

  auto SetData(const std::span<const uint8_t> &values) -> Error;
  auto SetData(const std::span<const char> &values) -> Error;
  auto GetData() -> std::span<const uint8_t>;

  [[nodiscard]] auto GetStageFlags() const -> VkShaderStageFlags;

  [[nodiscard]] auto ContainsUniform(const ResourceKey &key) const -> bool;
  [[nodiscard]] auto GetUniformOffset(const ResourceKey &key) const
      -> std::optional<uint32_t>;
  [[nodiscard]] auto GetUniform(const ResourceKey &key) const
      -> const Reflect::ResourceInfo *;

private:
  Reflect::FlattenedReflection layout;
  std::vector<uint8_t> data;
  VkShaderStageFlags stageFlags{VK_SHADER_STAGE_ALL};
};

} // namespace Graphics