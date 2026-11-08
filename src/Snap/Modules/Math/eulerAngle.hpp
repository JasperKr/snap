#pragma once

#include "mathTypes.hpp"
#include <cmath>
#include <numbers>
#include <span>
#include <string>

namespace Math {

struct EulerAngle {
  // Attitude, X-axis / Y-Z plane rotation
  Scalar pitch{};

  // Heading, Y-axis / X-Z plane rotation
  Scalar yaw{};

  // Bank, Z-axis / X-Y plane rotation
  Scalar roll{};

  // NOLINTNEXTLINE, easily swappable.
  constexpr EulerAngle(Scalar pitch_val, Scalar yaw_val, Scalar roll_val) : pitch(pitch_val), yaw(yaw_val), roll(roll_val) {}

  constexpr EulerAngle() = default;

  [[nodiscard]] auto ToString() const -> std::string {
    return "(Pitch: " + std::to_string(pitch) + ", Yaw: " + std::to_string(yaw) + ", Roll: " + std::to_string(roll) + ")";
  }

  [[nodiscard]] auto Span() const -> std::span<const Scalar, 3> { return std::span<const Scalar, 3>(&pitch, 3); }
  [[nodiscard]] auto Ptr() const -> const Scalar * { return &pitch; }
  [[nodiscard]] auto Ptr() -> Scalar * { return &pitch; }

  static constexpr auto DegToRad(Scalar degrees) -> Scalar {
    return degrees * static_cast<Scalar>(std::numbers::pi) / static_cast<Scalar>(180.0F); // NOLINT
  }

  static constexpr auto RadToDeg(Scalar radians) -> Scalar {
    return radians * static_cast<Scalar>(180.0F) / // NOLINT
           static_cast<Scalar>(std::numbers::pi);
  }

  [[nodiscard]] constexpr auto ToRadians() const -> EulerAngle { return {DegToRad(pitch), DegToRad(yaw), DegToRad(roll)}; }

  [[nodiscard]] constexpr auto ToDegrees() const -> EulerAngle { return {RadToDeg(pitch), RadToDeg(yaw), RadToDeg(roll)}; }

  constexpr auto SanitiseAsRadians() -> void {
    constexpr auto limit = static_cast<Scalar>(2.0F * std::numbers::pi);

    pitch = std::fmod(pitch, limit);
    yaw = std::fmod(yaw, limit);
    roll = std::fmod(roll, limit);
  }

  constexpr auto SanitiseAsDegrees() -> void {
    constexpr auto limit = static_cast<Scalar>(360.0F);

    pitch = std::fmod(pitch, limit);
    yaw = std::fmod(yaw, limit);
    roll = std::fmod(roll, limit);
  }
};

}; // namespace Math