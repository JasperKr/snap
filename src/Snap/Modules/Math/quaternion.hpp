#pragma once

#include "mathTypes.hpp"
#include "vector.hpp"
namespace Math {

struct Quaternion {
  float x{};
  float y{};
  float z{};
  float w{};

  constexpr Quaternion(float x_val, float y_val, float z_val, float w_val) : x(x_val), y(y_val), z(z_val), w(w_val) {}

  constexpr Quaternion() : w(1.0F) {}

  [[nodiscard]] auto Normalize() const -> Quaternion;
  [[nodiscard]] auto Conjugate() const -> Quaternion;
  [[nodiscard]] auto Inverse() const -> Quaternion;
  [[nodiscard]] auto Dot(const Quaternion &other) const -> float;
  [[nodiscard]] auto Multiply(const Quaternion &other) const -> Quaternion;
  [[nodiscard]] auto RotateVector(const Vec3 &vec) const -> Vec3;

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Span() const -> std::span<const float, 4> { return std::span<const float, 4>(&x, 4); }
  [[nodiscard]] auto Ptr() const -> const float * { return &x; }
  [[nodiscard]] auto Ptr() -> float * { return &x; }

  auto operator==(const Quaternion &other) const -> bool { return x == other.x && y == other.y && z == other.z && w == other.w; }

  auto operator!=(const Quaternion &other) const -> bool { return !(*this == other); }

  auto operator*(const Quaternion &other) const -> Quaternion { return Multiply(other); }

  auto operator*(const Vec3 &vec) const -> Vec3 { return RotateVector(vec); }
};

} // namespace Math