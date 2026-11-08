#pragma once

#include "Graphics/bufferformat.hpp"
#include "Modules/color.hpp"
#include <cstdint>
#include <flecs.h>
#include <imgui.h>

namespace Engine {

enum class IntensityUnit : uint8_t {
  // Candela; (lm/sr) aka (luminous flux per steradian)
  LuminousIntensity,

  // Lux; (lm/m^2) aka (luminous flux per square meter)
  Illuminance,
};

enum class LightType : uint8_t {
  None,
  Directional,
  Point,
  Spot,
  Rectangle,
  Sphere,
};

struct Light {
  Light() = default;
  Light(const Light &) = default;
  auto operator=(const Light &) -> Light & = default;
  Light(Light &&) = default;
  auto operator=(Light &&) -> Light & = default;
  ~Light();

  Color Color;
  float Intensity{};
  int32_t ShadowBufferIndex = -1;

  /// CPU only data ///
  IntensityUnit IntensityUnit = IntensityUnit::LuminousIntensity;
  int32_t BufferIndex = -1;
  LightType Type = LightType::None;

  static auto GetBufferFormat() -> Graphics::BufferFormat &;

  // NOLINTBEGIN
  auto Write(std::span<uint8_t> buffer, size_t offset) const -> size_t {
    assert(offset + (5UL * sizeof(float)) <= buffer.size());

    auto *floatData = reinterpret_cast<float *>(buffer.data() + offset);
    floatData[0] = Color.r;
    floatData[1] = Color.g;
    floatData[2] = Color.b;
    floatData[3] = Intensity;

    auto *int32Data = reinterpret_cast<int32_t *>(buffer.data() + offset + (4UL * sizeof(float)));
    int32Data[0] = ShadowBufferIndex;

    return 5UL * sizeof(float);
  }
  // NOLINTEND

  auto SetColor(const struct Color &color) -> void { Color = color; }
  auto SetColor(float red, float green, float blue) -> void {
    Color.r = red;
    Color.g = green;
    Color.b = blue;
  }
  auto SetIntensity(float intensity) -> void { Intensity = intensity; }
  [[nodiscard]] auto GetColor() const -> struct Color {
    return Color;
  } [[nodiscard]] auto GetIntensity() const -> float {
    return Intensity;
  }

  auto DrawGUI(flecs::entity entity) -> void {
    ImGui::ColorEdit3("Color", Color.Ptr());

    ImGui::InputFloat("Intensity", &Intensity, 0.1F, 100.0F, "%.3f"); // NOLINT
  }
};

} // namespace Engine