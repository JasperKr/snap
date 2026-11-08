#pragma once

#include "Modules/Math/math.hpp"
#include "float16_t/float16_t.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <unordered_map>

static constexpr float uint8_max_as_float = 255.0F;

// 32-bit per channel RGBA color representation
struct Color {
  float r;
  float g;
  float b;
  float a;

  constexpr Color() : r(0), g(0), b(0), a(1) {}

  // NOLINTNEXTLINE
  constexpr Color(float red, float green, float blue, float alpha = 1.0F) : r(red), g(green), b(blue), a(alpha) {}

  // NOLINTNEXTLINE
  constexpr Color(double red, double green, double blue, double alpha = 1.0)
      : r(static_cast<float>(red)), g(static_cast<float>(green)), b(static_cast<float>(blue)), a(static_cast<float>(alpha)) {}

  // NOLINTNEXTLINE
  constexpr Color(int red, int green, int blue, int alpha = 255)
      : r(static_cast<float>(red) / uint8_max_as_float), g(static_cast<float>(green) / uint8_max_as_float), b(static_cast<float>(blue) / uint8_max_as_float),
        a(static_cast<float>(alpha) / uint8_max_as_float) {}

  auto Ptr() -> float * { return &r; }
  [[nodiscard]] auto Ptr() const -> float const * { return &r; }

  static constexpr uint32_t RedShift = 24U;
  static constexpr uint32_t GreenShift = 16U;
  static constexpr uint32_t BlueShift = 8U;
  static constexpr uint32_t BitMask = 0xFFU;

  // NOLINTNEXTLINE
  constexpr Color(uint32_t packedRGBA) {
    uint8_t red = (packedRGBA >> RedShift) & BitMask;
    uint8_t green = (packedRGBA >> GreenShift) & BitMask;
    uint8_t blue = (packedRGBA >> BlueShift) & BitMask;
    uint8_t alpha = packedRGBA & BitMask;

    r = static_cast<float>(red) / uint8_max_as_float;
    g = static_cast<float>(green) / uint8_max_as_float;
    b = static_cast<float>(blue) / uint8_max_as_float;
    a = static_cast<float>(alpha) / uint8_max_as_float;
  };

  [[nodiscard]] auto Pack() const -> uint32_t {
    auto red = static_cast<float>(r) * uint8_max_as_float;
    auto green = static_cast<float>(g) * uint8_max_as_float;
    auto blue = static_cast<float>(b) * uint8_max_as_float;
    auto alpha = static_cast<float>(a) * uint8_max_as_float;

    return (static_cast<uint32_t>(red) << RedShift) | (static_cast<uint32_t>(green) << GreenShift) | (static_cast<uint32_t>(blue) << BlueShift) |
           static_cast<uint32_t>(alpha);
  }

  [[nodiscard]] auto ToGammaCorrect() const -> Color {
    // sRGB gamma correction
    static auto GammaCorrectChannel = [](float channel) -> float {
      // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
      if (channel <= 0.0031308F) {
        return 12.92F * channel;
      }

      return (1.055F * std::pow(channel, 1.0F / 2.4F)) - 0.055F;
      // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    };

    return {GammaCorrectChannel(r), GammaCorrectChannel(g), GammaCorrectChannel(b), a};
  };

  [[nodiscard]] auto ToLinear() const -> Color {
    // Inverse sRGB gamma correction
    static auto LinearizeChannel = [](float channel) -> float {
      // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
      if (channel <= 0.04045F) {
        return channel / 12.92F;
      }

      return std::pow((channel + 0.055F) / 1.055F, 2.4F);
      // NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    };

    return {LinearizeChannel(r), LinearizeChannel(g), LinearizeChannel(b), a};
  };

  auto operator==(const Color &other) const -> bool { return r == other.r && g == other.g && b == other.b && a == other.a; }

  auto operator!=(const Color &other) const -> bool { return !(*this == other); }

  auto operator*(float scalar) const -> Color { return {r * scalar, g * scalar, b * scalar, a * scalar}; }

  auto operator*(const Color &other) const -> Color { return {r * other.r, g * other.g, b * other.b, a * other.a}; }

  auto operator+(const Color &other) const -> Color { return {r + other.r, g + other.g, b + other.b, a + other.a}; }

  auto operator-(const Color &other) const -> Color { return {r - other.r, g - other.g, b - other.b, a - other.a}; }

  [[nodiscard]] auto Luminance() const -> float {
    // Perceived brightness calculation using the Rec. 709 formula NOLINTNEXTLINE
    return (r * 0.2126F) + (g * 0.7152F) + (b * 0.0722F);
  }

  auto operator<(const Color &other) const -> bool { return Luminance() < other.Luminance(); }
  auto operator>(const Color &other) const -> bool { return Luminance() > other.Luminance(); }
  auto operator<=(const Color &other) const -> bool { return Luminance() <= other.Luminance(); }
  auto operator>=(const Color &other) const -> bool { return Luminance() >= other.Luminance(); }

  auto operator*(float scalar) -> Color & {
    r *= scalar;
    g *= scalar;
    b *= scalar;
    a *= scalar;
    return *this;
  }

  auto operator*=(const Color &other) -> Color & {
    r *= other.r;
    g *= other.g;
    b *= other.b;
    a *= other.a;
    return *this;
  }

  auto operator+=(const Color &other) -> Color & {
    r += other.r;
    g += other.g;
    b += other.b;
    a += other.a;
    return *this;
  }

  auto operator-=(const Color &other) -> Color & {
    r -= other.r;
    g -= other.g;
    b -= other.b;
    a -= other.a;
    return *this;
  }

  // Inputs RGB in the range [0, 1], outputs HSV in the range [0, 1] for S and V, and [0, 360] for H
  [[nodiscard]] auto ToHSV() const -> Color {
    float max = std::max({r, g, b});
    float min = std::min({r, g, b});
    float delta = max - min;

    // NOLINTBEGIN

    float hue = 0.0F;
    if (delta > 0.0F) {
      if (max == r) {
        hue = 60.0F * (fmod(((g - b) / delta), 6.0F));
      } else if (max == g) {
        hue = 60.0F * (((b - r) / delta) + 2.0F);
      } else if (max == b) {
        hue = 60.0F * (((r - g) / delta) + 4.0F);
      }
    }

    float saturation = (max == 0.0F) ? 0.0F : (delta / max);
    float value = max;

    // NOLINTEND

    return {hue, saturation, value};
  }

  // Inputs RGB in the range [0, 1], outputs HSL in the range [0, 1] for S and L, and [0, 360] for H
  [[nodiscard]] auto ToHSL() const -> Color {
    float max = std::max({r, g, b});
    float min = std::min({r, g, b});
    float delta = max - min;

    // NOLINTBEGIN

    float lightness = (max + min) / 2.0F;

    float hue = 0.0F;
    float saturation = 0.0F;

    if (delta > 0.0F) {
      saturation = delta / (1.0F - std::fabs(2.0F * lightness - 1.0F));

      if (max == r) {
        hue = 60.0F * (fmod(((g - b) / delta), 6.0F));
      } else if (max == g) {
        hue = 60.0F * (((b - r) / delta) + 2.0F);
      } else if (max == b) {
        hue = 60.0F * (((r - g) / delta) + 4.0F);
      }
    }

    // NOLINTEND

    return {hue, saturation, lightness};
  }

  // Converts HSV to RGB. Expects H in [0, 360], S and V in [0, 1]. Returns RGB in [0, 1].
  [[nodiscard]] static auto FromHSV(Color hsv) -> Color {
    // NOLINTBEGIN
    float hue = std::fmod(hsv.r, 360.0F);
    if (hue < 0.0F) {
      hue += 360.0F;
    }

    float saturation = std::clamp(hsv.g, 0.0F, 1.0F);
    float value = std::clamp(hsv.b, 0.0F, 1.0F);

    float chroma = value * saturation;
    float x = chroma * (1.0F - std::fabs(std::fmod(hue / 60.0F, 2.0F) - 1.0F));
    float m = value - chroma;

    float redPrime = 0.0F;
    float greenPrime = 0.0F;
    float bluePrime = 0.0F;

    if (hue < 60.0F) {
      redPrime = chroma;
      greenPrime = x;
    } else if (hue < 120.0F) {
      redPrime = x;
      greenPrime = chroma;
    } else if (hue < 180.0F) {
      greenPrime = chroma;
      bluePrime = x;
    } else if (hue < 240.0F) {
      greenPrime = x;
      bluePrime = chroma;
    } else if (hue < 300.0F) {
      redPrime = x;
      bluePrime = chroma;
    } else {
      redPrime = chroma;
      bluePrime = x;
    }

    return {redPrime + m, greenPrime + m, bluePrime + m, hsv.a};
    // NOLINTEND
  }

  // Converts HSL to RGB. Expects H in [0, 360], S and L in [0, 1]. Returns RGB in [0, 1].
  [[nodiscard]] static auto FromHSL(Color hsl) -> Color {
    // NOLINTBEGIN
    float hue = std::fmod(hsl.r, 360.0F);
    if (hue < 0.0F) {
      hue += 360.0F;
    }

    float saturation = std::clamp(hsl.g, 0.0F, 1.0F);
    float lightness = std::clamp(hsl.b, 0.0F, 1.0F);

    float chroma = (1.0F - std::fabs((2.0F * lightness) - 1.0F)) * saturation;
    float x = chroma * (1.0F - std::fabs(std::fmod(hue / 60.0F, 2.0F) - 1.0F));
    float m = lightness - (chroma / 2.0F);

    float redPrime = 0.0F;
    float greenPrime = 0.0F;
    float bluePrime = 0.0F;

    if (hue < 60.0F) {
      redPrime = chroma;
      greenPrime = x;
    } else if (hue < 120.0F) {
      redPrime = x;
      greenPrime = chroma;
    } else if (hue < 180.0F) {
      greenPrime = chroma;
      bluePrime = x;
    } else if (hue < 240.0F) {
      greenPrime = x;
      bluePrime = chroma;
    } else if (hue < 300.0F) {
      redPrime = x;
      bluePrime = chroma;
    } else {
      redPrime = chroma;
      bluePrime = x;
    }

    return {redPrime + m, greenPrime + m, bluePrime + m, hsl.a};
    // NOLINTEND
  }

  static auto RandomColor(const std::string_view &seed) -> Color {
    static std::unordered_map<std::string_view, Color> colorCache;
    auto iter = colorCache.find(seed);
    if (iter != colorCache.end()) {
      return iter->second;
    }

    float randomHue = Math::Random(0.0F, 360.0F);        // NOLINT
    float randomSaturation = Math::Random(0.25F, 0.75F); // NOLINT
    float randomValue = Math::Random(0.2F, 0.75F);       // NOLINT

    Color randomColor = Color::FromHSV({randomHue, randomSaturation, randomValue, 1.0F});
    colorCache[seed] = randomColor;

    return randomColor;
  }

  // Ease of use for Vk colors
  auto FillFloatArray(float *array) const -> void {
    // NOLINTBEGIN
    array[0] = r;
    array[1] = g;
    array[2] = b;
    array[3] = a;
    // NOLINTEND
  }
};