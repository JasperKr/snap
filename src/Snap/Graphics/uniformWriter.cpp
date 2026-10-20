#include "uniformWriter.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <vulkan/vulkan_core.h>

namespace Graphics {
auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const std::span<const uint8_t> &data) -> Error {
  return shader->Send(key, data);
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         float value) -> Error {
  return shader->Send(key, SpanGenericType<float>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         int value) -> Error {
  return shader->Send(key, SpanGenericType<int>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         uint32_t value) -> Error {
  return shader->Send(key, SpanGenericType<uint32_t>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         bool value) -> Error {
  return shader->Send(key, SpanGenericType<uint32_t>::GetSpan(
                               static_cast<const unsigned int>(value)));
}

// TODO: This completely breaks if we set the engine to double precision

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Vec2 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Vec2>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Vec3 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Vec3>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Vec4 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Vec4>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Uvec2 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Uvec2>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Uvec3 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Uvec3>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Uvec4 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Uvec4>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Ivec2 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Ivec2>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Ivec3 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Ivec3>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Ivec4 &value) -> Error {
  return shader->Send(key, SpanGenericType<Math::Ivec4>::GetSpan(value));
}

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Matrix4x4 &value) -> Error {
  const auto &span = value.byteSpan();
  return shader->Send(key, span);
};

auto UniformWriter::Send(const Ref<Shader> &shader, const ResourceKey &key,
                         const Math::Matrix3x3 &value) -> Error {
  // Matrix4x3 since uniforms are std140 aligned
  thread_local std::array<float, 12> matrix3x3Data; // NOLINT
  thread_local std::span<uint8_t> matrix3x3Span(
      reinterpret_cast<uint8_t *>(matrix3x3Data.data()), // NOLINT
      matrix3x3Data.size() * sizeof(float));

#pragma unroll
  for (int row = 0; row < Math::Matrix3x3::Rows; row++) {
#pragma unroll
    for (int col = 0; col < Math::Matrix3x3::Cols; col++) {
      matrix3x3Data.at((row * 4) + col) = value.At(row, col);
    }
  }

  return shader->Send(key,
                      matrix3x3Span); // NOLINT
};
} // namespace Graphics