#pragma once

#include "Graphics/shader.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Graphics {

struct UniformWriter {
  template <typename T> struct SpanGenericType {
    static auto GetSpan(const T &value) -> std::span<const uint8_t> {
      // NOLINTNEXTLINE
      return {reinterpret_cast<const uint8_t *>(&value), sizeof(T)};
    };
  };

  static auto Send(const GraphicsContext &context, const Ref<Shader> &shader,
                   const ResourceKey &key, const Ref<Buffer> &buffer,
                   VkDescriptorType type) -> Error;

  static auto Send(const GraphicsContext &context, const Ref<Shader> &shader,
                   const ResourceKey &key,
                   const Ref<Graphics::StructuredBuffer> &buffer,
                   VkDescriptorType type) -> Error;

  static auto Send(const GraphicsContext &context, const Ref<Shader> &shader,
                   const ResourceKey &key,
                   const Ref<Graphics::Texture> &texture, VkDescriptorType type)
      -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const std::span<const uint8_t> &data) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   float value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key, int value)
      -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   uint32_t value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   bool value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Vec2 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Vec3 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Vec4 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Uvec2 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Uvec3 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Uvec4 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Ivec2 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Ivec3 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Ivec4 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Matrix4x4 &value) -> Error;

  static auto Send(const Ref<Shader> &shader, const ResourceKey &key,
                   const Math::Matrix3x3 &value) -> Error;
};

} // namespace Graphics