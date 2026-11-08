#include "vector.hpp"
#include "Modules/Helpers/hasher.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <sstream>

namespace Math {
// Index operator overloads for all vector types

// NOLINTNEXTLINE
thread_local inline Hash::Hasher Hasher;

[[nodiscard]] auto Vec2::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Vec2(" << x << ", " << y << ")";
  return oss.str();
}

[[nodiscard]] auto Vec2::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<float>()(x));
  Hasher.Add(std::hash<float>()(y));
  return Hasher.Get();
}

[[nodiscard]] auto Vec3::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<float>()(x));
  Hasher.Add(std::hash<float>()(y));
  Hasher.Add(std::hash<float>()(z));
  return Hasher.Get();
}

[[nodiscard]] auto Vec4::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<float>()(x));
  Hasher.Add(std::hash<float>()(y));
  Hasher.Add(std::hash<float>()(z));
  Hasher.Add(std::hash<float>()(w));
  return Hasher.Get();
}

[[nodiscard]] auto Uvec2::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<uint32_t>()(x));
  Hasher.Add(std::hash<uint32_t>()(y));
  return Hasher.Get();
}

[[nodiscard]] auto Uvec3::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<uint32_t>()(x));
  Hasher.Add(std::hash<uint32_t>()(y));
  Hasher.Add(std::hash<uint32_t>()(z));
  return Hasher.Get();
}

[[nodiscard]] auto Uvec4::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<uint32_t>()(x));
  Hasher.Add(std::hash<uint32_t>()(y));
  Hasher.Add(std::hash<uint32_t>()(z));
  Hasher.Add(std::hash<uint32_t>()(w));
  return Hasher.Get();
}

[[nodiscard]] auto Ivec2::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<int32_t>()(x));
  Hasher.Add(std::hash<int32_t>()(y));
  return Hasher.Get();
}

[[nodiscard]] auto Ivec3::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<int32_t>()(x));
  Hasher.Add(std::hash<int32_t>()(y));
  Hasher.Add(std::hash<int32_t>()(z));
  return Hasher.Get();
}

[[nodiscard]] auto Ivec4::Hash() const -> uint64_t {
  Hasher.Reset();
  Hasher.Add(std::hash<int32_t>()(x));
  Hasher.Add(std::hash<int32_t>()(y));
  Hasher.Add(std::hash<int32_t>()(z));
  Hasher.Add(std::hash<int32_t>()(w));
  return Hasher.Get();
}

[[nodiscard]] auto Vec3::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Vec3(" << x << ", " << y << ", " << z << ")";
  return oss.str();
}

[[nodiscard]] auto Vec4::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Vec4(" << x << ", " << y << ", " << z << ", " << w << ")";
  return oss.str();
}

[[nodiscard]] auto Uvec2::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Uvec2(" << x << ", " << y << ")";
  return oss.str();
}

[[nodiscard]] auto Uvec3::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Uvec3(" << x << ", " << y << ", " << z << ")";
  return oss.str();
}

[[nodiscard]] auto Uvec4::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Uvec4(" << x << ", " << y << ", " << z << ", " << w << ")";
  return oss.str();
}

[[nodiscard]] auto Ivec2::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Ivec2(" << x << ", " << y << ")";
  return oss.str();
}

[[nodiscard]] auto Ivec3::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Ivec3(" << x << ", " << y << ", " << z << ")";
  return oss.str();
}

[[nodiscard]] auto Ivec4::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Ivec4(" << x << ", " << y << ", " << z << ", " << w << ")";
  return oss.str();
}

auto Vec2::operator[](uint32_t index) -> float & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  default:
    assert(false && "Vec2 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

auto Vec2::operator[](uint32_t index) const -> float {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  default:
    assert(false && "Vec2 index out of range");
    return 0;
  }
}

auto Vec3::operator[](uint32_t index) -> float & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  default:
    assert(false && "Vec3 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Vec3
auto Vec3::operator[](uint32_t index) const -> float {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  default:
    assert(false && "Vec3 index out of range");
    return 0;
  }
}

// Vec4
auto Vec4::operator[](uint32_t index) -> float & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    assert(false && "Vec4 index out of range");
  }
  return x; // Return x as a fallback to silence compiler warning
}

// Vec4
auto Vec4::operator[](uint32_t index) const -> float {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    assert(false && "Vec4 index out of range");
    return 0;
  }
}

auto Uvec2::operator[](uint32_t index) -> uint32_t & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  default:
    assert(false && "Uvec2 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Uvec2
auto Uvec2::operator[](uint32_t index) const -> uint32_t {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  default:
    assert(false && "Uvec2 index out of range");
    return 0;
  }
}

auto Uvec3::operator[](uint32_t index) -> uint32_t & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  default:
    assert(false && "Uvec3 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Uvec3
auto Uvec3::operator[](uint32_t index) const -> uint32_t {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  default:
    assert(false && "Uvec3 index out of range");
    return 0;
  }
}

auto Uvec4::operator[](uint32_t index) -> uint32_t & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    assert(false && "Uvec4 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Uvec4
auto Uvec4::operator[](uint32_t index) const -> uint32_t {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    assert(false && "Uvec4 index out of range");
    return 0;
  }
}

auto Ivec2::operator[](uint32_t index) -> int32_t & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  default:
    assert(false && "Ivec2 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Ivec2
auto Ivec2::operator[](uint32_t index) const -> int32_t {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  default:
    assert(false && "Ivec2 index out of range");
    return 0;
  }
}

auto Ivec3::operator[](uint32_t index) -> int32_t & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  default:
    assert(false && "Ivec3 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Ivec3
auto Ivec3::operator[](uint32_t index) const -> int32_t {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  default:
    assert(false && "Ivec3 index out of range");
    return 0;
  }
}

auto Ivec4::operator[](uint32_t index) -> int32_t & {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    assert(false && "Ivec4 index out of range");
    return x; // Return x as a fallback to silence compiler warning
  }
}

// Ivec4
auto Ivec4::operator[](uint32_t index) const -> int32_t {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    assert(false && "Ivec4 index out of range");
    return 0;
  }
}

static constexpr float Epsilon = 1e-6F;

///// Vec2 /////

/// Addition ///
auto Vec2::operator+(const Vec2 &other) const -> Vec2 { return {x + other.x, y + other.y}; }
auto Vec2::operator+(float scalar) const -> Vec2 { return {x + scalar, y + scalar}; }
auto Vec2::operator+=(const Vec2 &other) -> Vec2 & {
  x += other.x;
  y += other.y;
  return *this;
}
auto Vec2::operator+=(float scalar) -> Vec2 & {
  x += scalar;
  y += scalar;
  return *this;
}

/// Subtraction ///
auto Vec2::operator-(const Vec2 &other) const -> Vec2 { return {x - other.x, y - other.y}; }
auto Vec2::operator-(float scalar) const -> Vec2 { return {x - scalar, y - scalar}; }
auto Vec2::operator-=(const Vec2 &other) -> Vec2 & {
  x -= other.x;
  y -= other.y;
  return *this;
}
auto Vec2::operator-=(float scalar) -> Vec2 & {
  x -= scalar;
  y -= scalar;
  return *this;
}
auto Vec2::operator-() const -> Vec2 { return {-x, -y}; }

/// Multiplication ///
auto Vec2::operator*(float scalar) const -> Vec2 { return {x * scalar, y * scalar}; }
auto Vec2::operator*(const Vec2 &other) const -> Vec2 { return {x * other.x, y * other.y}; }
auto Vec2::operator*=(float scalar) -> Vec2 & {
  x *= scalar;
  y *= scalar;
  return *this;
}
auto Vec2::operator*=(const Vec2 &other) -> Vec2 & {
  x *= other.x;
  y *= other.y;
  return *this;
}

/// Division ///
auto Vec2::operator/(float scalar) const -> Vec2 { return {x / scalar, y / scalar}; }
auto Vec2::operator/(const Vec2 &other) const -> Vec2 { return {x / other.x, y / other.y}; }
auto Vec2::operator/=(float scalar) -> Vec2 & {
  x /= scalar;
  y /= scalar;
  return *this;
}
auto Vec2::operator/=(const Vec2 &other) -> Vec2 & {
  x /= other.x;
  y /= other.y;
  return *this;
}

auto Vec2::operator==(const Vec2 &other) const -> bool { return x == other.x && y == other.y; }
auto Vec2::operator!=(const Vec2 &other) const -> bool { return !(*this == other); }
auto Vec2::Length() const -> float { return std::sqrt((x * x) + (y * y)); }
auto Vec2::LengthSqr() const -> float { return (x * x) + (y * y); }
auto Vec2::Distance(const Vec2 &other) const -> float { return std::sqrt(((x - other.x) * (x - other.x)) + ((y - other.y) * (y - other.y))); }
auto Vec2::DistanceSqr(const Vec2 &other) const -> float { return ((x - other.x) * (x - other.x)) + ((y - other.y) * (y - other.y)); }
auto Vec2::Inverse() const -> Vec2 { return {1.0F / x, 1.0F / y}; }
auto Vec2::Normalize() const -> Vec2 {
  float len = Length();
  if (len <= Epsilon) {
    return {0.0F, 0.0F};
  }
  return {x / len, y / len};
}
auto Vec2::Dot(const Vec2 &other) const -> float { return (x * other.x) + (y * other.y); }
auto Vec2::Cross(const Vec2 &other) const -> Vec2 { return {(x * other.y) - (y * other.x), (y * other.x) - (x * other.y)}; }
auto Vec2::Max(const Vec2 &other) const -> Vec2 { return {std::fmax(x, other.x), std::fmax(y, other.y)}; }
auto Vec2::Max(float scalar) const -> Vec2 { return {std::fmax(x, scalar), std::fmax(y, scalar)}; }
auto Vec2::Min(const Vec2 &other) const -> Vec2 { return {std::fmin(x, other.x), std::fmin(y, other.y)}; }
auto Vec2::Min(float scalar) const -> Vec2 { return {std::fmin(x, scalar), std::fmin(y, scalar)}; }
Vec2::Vec2(const Vec3 &vec3) : x(vec3.x), y(vec3.y) {}
Vec2::Vec2(const Vec4 &vec4) : x(vec4.x), y(vec4.y) {}
Vec2::Vec2(const Ivec2 &vec2) : x(static_cast<float>(vec2.x)), y(static_cast<float>(vec2.y)) {}
Vec2::Vec2(const Ivec3 &vec3) : x(static_cast<float>(vec3.x)), y(static_cast<float>(vec3.y)) {}
Vec2::Vec2(const Ivec4 &vec4) : x(static_cast<float>(vec4.x)), y(static_cast<float>(vec4.y)) {}
Vec2::Vec2(const Uvec2 &vec2) : x(static_cast<float>(vec2.x)), y(static_cast<float>(vec2.y)) {}
Vec2::Vec2(const Uvec3 &vec3) : x(static_cast<float>(vec3.x)), y(static_cast<float>(vec3.y)) {}
Vec2::Vec2(const Uvec4 &vec4) : x(static_cast<float>(vec4.x)), y(static_cast<float>(vec4.y)) {}

///// Vec3 /////

/// Addition ///
auto Vec3::operator+(const Vec3 &other) const -> Vec3 { return {x + other.x, y + other.y, z + other.z}; }
auto Vec3::operator+(float scalar) const -> Vec3 { return {x + scalar, y + scalar, z + scalar}; }
auto Vec3::operator+=(const Vec3 &other) -> Vec3 & {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}
auto Vec3::operator+=(float scalar) -> Vec3 & {
  x += scalar;
  y += scalar;
  z += scalar;
  return *this;
}

/// Subtraction ///
auto Vec3::operator-(const Vec3 &other) const -> Vec3 { return {x - other.x, y - other.y, z - other.z}; }
auto Vec3::operator-(float scalar) const -> Vec3 { return {x - scalar, y - scalar, z - scalar}; }
auto Vec3::operator-=(const Vec3 &other) -> Vec3 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}
auto Vec3::operator-=(float scalar) -> Vec3 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  return *this;
}
auto Vec3::operator-() const -> Vec3 { return {-x, -y, -z}; }

/// Multiplication ///
auto Vec3::operator*(float scalar) const -> Vec3 { return {x * scalar, y * scalar, z * scalar}; }
auto Vec3::operator*(const Vec3 &other) const -> Vec3 { return {x * other.x, y * other.y, z * other.z}; }
auto Vec3::operator*=(float scalar) -> Vec3 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}
auto Vec3::operator*=(const Vec3 &other) -> Vec3 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  return *this;
}

/// Division ///
auto Vec3::operator/(float scalar) const -> Vec3 { return {x / scalar, y / scalar, z / scalar}; }
auto Vec3::operator/(const Vec3 &other) const -> Vec3 { return {x / other.x, y / other.y, z / other.z}; }
auto Vec3::operator/=(float scalar) -> Vec3 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  return *this;
}
auto Vec3::operator/=(const Vec3 &other) -> Vec3 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  return *this;
}

auto Vec3::operator==(const Vec3 &other) const -> bool { return x == other.x && y == other.y && z == other.z; }
auto Vec3::operator!=(const Vec3 &other) const -> bool { return !(*this == other); }
auto Vec3::Length() const -> float { return std::sqrt((x * x) + (y * y) + (z * z)); }
auto Vec3::LengthSqr() const -> float { return (x * x) + (y * y) + (z * z); }
auto Vec3::Distance(const Vec3 &other) const -> float {
  return std::sqrt(((x - other.x) * (x - other.x)) + ((y - other.y) * (y - other.y)) + ((z - other.z) * (z - other.z)));
}
auto Vec3::DistanceSqr(const Vec3 &other) const -> float {
  return ((x - other.x) * (x - other.x)) + ((y - other.y) * (y - other.y)) + ((z - other.z) * (z - other.z));
}
auto Vec3::Inverse() const -> Vec3 { return {1.0F / x, 1.0F / y, 1.0F / z}; }
auto Vec3::Normalize() const -> Vec3 {
  float len = Length();
  if (len <= Epsilon) {
    return {0.0F, 0.0F, 0.0F};
  }
  return {x / len, y / len, z / len};
}
auto Vec3::Dot(const Vec3 &other) const -> float { return (x * other.x) + (y * other.y) + (z * other.z); }
auto Vec3::Cross(const Vec3 &other) const -> Vec3 { return {(y * other.z) - (z * other.y), (z * other.x) - (x * other.z), (x * other.y) - (y * other.x)}; }
auto Vec3::Max(const Vec3 &other) const -> Vec3 { return {std::fmax(x, other.x), std::fmax(y, other.y), std::fmax(z, other.z)}; }
auto Vec3::Max(float scalar) const -> Vec3 { return {std::fmax(x, scalar), std::fmax(y, scalar), std::fmax(z, scalar)}; }
auto Vec3::Min(const Vec3 &other) const -> Vec3 { return {std::fmin(x, other.x), std::fmin(y, other.y), std::fmin(z, other.z)}; }
auto Vec3::Min(float scalar) const -> Vec3 { return {std::fmin(x, scalar), std::fmin(y, scalar), std::fmin(z, scalar)}; }
Vec3::Vec3(const Vec2 &vec2, float z_val) : x(vec2.x), y(vec2.y), z(z_val) {}
Vec3::Vec3(const Vec4 &vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}
Vec3::Vec3(const Ivec2 &vec2, float z_val) : x(static_cast<float>(vec2.x)), y(static_cast<float>(vec2.y)), z(z_val) {}
Vec3::Vec3(const Ivec3 &vec3) : x(static_cast<float>(vec3.x)), y(static_cast<float>(vec3.y)), z(static_cast<float>(vec3.z)) {}
Vec3::Vec3(const Ivec4 &vec4) : x(static_cast<float>(vec4.x)), y(static_cast<float>(vec4.y)), z(static_cast<float>(vec4.z)) {}
Vec3::Vec3(const Uvec2 &vec2, float z_val) : x(static_cast<float>(vec2.x)), y(static_cast<float>(vec2.y)), z(z_val) {}
Vec3::Vec3(const Uvec3 &vec3) : x(static_cast<float>(vec3.x)), y(static_cast<float>(vec3.y)), z(static_cast<float>(vec3.z)) {}
Vec3::Vec3(const Uvec4 &vec4) : x(static_cast<float>(vec4.x)), y(static_cast<float>(vec4.y)), z(static_cast<float>(vec4.z)) {}
Vec3::Vec3(const Vec4 &&vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}

[[nodiscard]] auto Vec3::GetTangent() const -> Vec3 {
  Vec3 tangent = {0.0F, 1.0F, 0.0F};
  if (tangent.Dot(*this) > 0.99F) { // NOLINT
    tangent = {1.0F, 0.0F, 0.0F};
  }
  return tangent.Cross(*this).Normalize();
}

///// Vec4 /////

/// Addition ///
auto Vec4::operator+(const Vec4 &other) const -> Vec4 { return {x + other.x, y + other.y, z + other.z, w + other.w}; }
auto Vec4::operator+(float scalar) const -> Vec4 { return {x + scalar, y + scalar, z + scalar, w + scalar}; }
auto Vec4::operator+=(const Vec4 &other) -> Vec4 & {
  x += other.x;
  y += other.y;
  z += other.z;
  w += other.w;
  return *this;
}
auto Vec4::operator+=(float scalar) -> Vec4 & {
  x += scalar;
  y += scalar;
  z += scalar;
  w += scalar;
  return *this;
}

/// Subtraction ///
auto Vec4::operator-(const Vec4 &other) const -> Vec4 { return {x - other.x, y - other.y, z - other.z, w - other.w}; }
auto Vec4::operator-(float scalar) const -> Vec4 { return {x - scalar, y - scalar, z - scalar, w - scalar}; }
auto Vec4::operator-=(const Vec4 &other) -> Vec4 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  w -= other.w;
  return *this;
}
auto Vec4::operator-=(float scalar) -> Vec4 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  w -= scalar;
  return *this;
}
auto Vec4::operator-() const -> Vec4 { return {-x, -y, -z, -w}; }

/// Multiplication ///
auto Vec4::operator*(float scalar) const -> Vec4 { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
auto Vec4::operator*(const Vec4 &other) const -> Vec4 { return {x * other.x, y * other.y, z * other.z, w * other.w}; }
auto Vec4::operator*=(float scalar) -> Vec4 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
  return *this;
}
auto Vec4::operator*=(const Vec4 &other) -> Vec4 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  w *= other.w;
  return *this;
}

/// Division ///
auto Vec4::operator/(float scalar) const -> Vec4 { return {x / scalar, y / scalar, z / scalar, w / scalar}; }
auto Vec4::operator/(const Vec4 &other) const -> Vec4 { return {x / other.x, y / other.y, z / other.z, w / other.w}; }
auto Vec4::operator/=(float scalar) -> Vec4 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  w /= scalar;
  return *this;
}
auto Vec4::operator/=(const Vec4 &other) -> Vec4 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  w /= other.w;
  return *this;
}

auto Vec4::operator==(const Vec4 &other) const -> bool { return x == other.x && y == other.y && z == other.z && w == other.w; }
auto Vec4::operator!=(const Vec4 &other) const -> bool { return !(*this == other); }
auto Vec4::Length() const -> float { return std::sqrt((x * x) + (y * y) + (z * z) + (w * w)); }
auto Vec4::LengthSqr() const -> float { return (x * x) + (y * y) + (z * z) + (w * w); }
auto Vec4::Distance(const Vec4 &other) const -> float {
  return std::sqrt(((x - other.x) * (x - other.x)) + ((y - other.y) * (y - other.y)) + ((z - other.z) * (z - other.z)) + ((w - other.w) * (w - other.w)));
}
auto Vec4::DistanceSqr(const Vec4 &other) const -> float {
  return ((x - other.x) * (x - other.x)) + ((y - other.y) * (y - other.y)) + ((z - other.z) * (z - other.z)) + ((w - other.w) * (w - other.w));
}

auto Vec4::Inverse() const -> Vec4 { return {1.0F / x, 1.0F / y, 1.0F / z, 1.0F / w}; }
auto Vec4::Normalize() const -> Vec4 {
  float len = Length();
  if (len <= Epsilon) {
    return {0.0F, 0.0F, 0.0F, 0.0F};
  }
  return {x / len, y / len, z / len, w / len};
}
auto Vec4::Cross(const Vec4 &other) const -> Vec3 { return {(y * other.z) - (z * other.y), (z * other.x) - (x * other.z), (x * other.y) - (y * other.x)}; }
auto Vec4::Dot(const Vec4 &other) const -> float { return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w); }
auto Vec4::Dot(const Vec3 &other) const -> float { return (x * other.x) + (y * other.y) + (z * other.z); }
auto Vec4::Max(const Vec4 &other) const -> Vec4 { return {std::fmax(x, other.x), std::fmax(y, other.y), std::fmax(z, other.z), std::fmax(w, other.w)}; }
auto Vec4::Max(float scalar) const -> Vec4 { return {std::fmax(x, scalar), std::fmax(y, scalar), std::fmax(z, scalar), std::fmax(w, scalar)}; }
auto Vec4::Min(const Vec4 &other) const -> Vec4 { return {std::fmin(x, other.x), std::fmin(y, other.y), std::fmin(z, other.z), std::fmin(w, other.w)}; }
auto Vec4::Min(float scalar) const -> Vec4 { return {std::fmin(x, scalar), std::fmin(y, scalar), std::fmin(z, scalar), std::fmin(w, scalar)}; }
Vec4::Vec4(const Vec2 &vec2, float z_val, float w_val) : x(vec2.x), y(vec2.y), z(z_val), w(w_val) {}
Vec4::Vec4(const Vec3 &vec3, float w_val) : x(vec3.x), y(vec3.y), z(vec3.z), w(w_val) {}
Vec4::Vec4(const Ivec2 &vec2, float z_val, float w_val) : x(static_cast<float>(vec2.x)), y(static_cast<float>(vec2.y)), z(z_val), w(w_val) {}
Vec4::Vec4(const Ivec3 &vec3, float w_val) : x(static_cast<float>(vec3.x)), y(static_cast<float>(vec3.y)), z(static_cast<float>(vec3.z)), w(w_val) {}
Vec4::Vec4(const Ivec4 &vec4) : x(static_cast<float>(vec4.x)), y(static_cast<float>(vec4.y)), z(static_cast<float>(vec4.z)), w(static_cast<float>(vec4.w)) {}
Vec4::Vec4(const Uvec2 &vec2, float z_val, float w_val) : x(static_cast<float>(vec2.x)), y(static_cast<float>(vec2.y)), z(z_val), w(w_val) {}
Vec4::Vec4(const Uvec3 &vec3, float w_val) : x(static_cast<float>(vec3.x)), y(static_cast<float>(vec3.y)), z(static_cast<float>(vec3.z)), w(w_val) {}
Vec4::Vec4(const Uvec4 &vec4) : x(static_cast<float>(vec4.x)), y(static_cast<float>(vec4.y)), z(static_cast<float>(vec4.z)), w(static_cast<float>(vec4.w)) {}

///// Uvec2 /////

/// Addition ///
auto Uvec2::operator+(const Uvec2 &other) const -> Uvec2 { return {x + other.x, y + other.y}; }
auto Uvec2::operator+(uint32_t scalar) const -> Uvec2 { return {x + scalar, y + scalar}; }
auto Uvec2::operator+=(const Uvec2 &other) -> Uvec2 & {
  x += other.x;
  y += other.y;
  return *this;
}
auto Uvec2::operator+=(uint32_t scalar) -> Uvec2 & {
  x += scalar;
  y += scalar;
  return *this;
}

/// Subtraction ///
auto Uvec2::operator-(const Uvec2 &other) const -> Uvec2 { return {x - other.x, y - other.y}; }
auto Uvec2::operator-(uint32_t scalar) const -> Uvec2 { return {x - scalar, y - scalar}; }
auto Uvec2::operator-=(const Uvec2 &other) -> Uvec2 & {
  x -= other.x;
  y -= other.y;
  return *this;
}
auto Uvec2::operator-=(uint32_t scalar) -> Uvec2 & {
  x -= scalar;
  y -= scalar;
  return *this;
}

/// Multiplication ///
auto Uvec2::operator*(uint32_t scalar) const -> Uvec2 { return {x * scalar, y * scalar}; }
auto Uvec2::operator*(const Uvec2 &other) const -> Uvec2 { return {x * other.x, y * other.y}; }
auto Uvec2::operator*=(uint32_t scalar) -> Uvec2 & {
  x *= scalar;
  y *= scalar;
  return *this;
}
auto Uvec2::operator*=(const Uvec2 &other) -> Uvec2 & {
  x *= other.x;
  y *= other.y;
  return *this;
}

/// Division ///
auto Uvec2::operator/(uint32_t scalar) const -> Uvec2 { return {x / scalar, y / scalar}; }
auto Uvec2::operator/(const Uvec2 &other) const -> Uvec2 { return {x / other.x, y / other.y}; }
auto Uvec2::operator/=(uint32_t scalar) -> Uvec2 & {
  x /= scalar;
  y /= scalar;
  return *this;
}
auto Uvec2::operator/=(const Uvec2 &other) -> Uvec2 & {
  x /= other.x;
  y /= other.y;
  return *this;
}

/// Modulo ///
auto Uvec2::operator%(uint32_t scalar) const -> Uvec2 { return {x % scalar, y % scalar}; }
auto Uvec2::operator%(const Uvec2 &other) const -> Uvec2 { return {x % other.x, y % other.y}; }
auto Uvec2::operator%=(uint32_t scalar) -> Uvec2 & {
  x %= scalar;
  y %= scalar;
  return *this;
}
auto Uvec2::operator%=(const Uvec2 &other) -> Uvec2 & {
  x %= other.x;
  y %= other.y;
  return *this;
}

auto Uvec2::operator==(const Uvec2 &other) const -> bool { return x == other.x && y == other.y; }
auto Uvec2::operator!=(const Uvec2 &other) const -> bool { return !(*this == other); }
Uvec2::Uvec2(const Uvec3 &vec3) : x(vec3.x), y(vec3.y) {}
Uvec2::Uvec2(const Uvec4 &vec4) : x(vec4.x), y(vec4.y) {}
Uvec2::Uvec2(const Ivec2 &vec2) : x(static_cast<uint32_t>(vec2.x)), y(static_cast<uint32_t>(vec2.y)) {}
Uvec2::Uvec2(const Ivec3 &vec3) : x(static_cast<uint32_t>(vec3.x)), y(static_cast<uint32_t>(vec3.y)) {}
Uvec2::Uvec2(const Ivec4 &vec4) : x(static_cast<uint32_t>(vec4.x)), y(static_cast<uint32_t>(vec4.y)) {}

///// Uvec3 /////

/// Addition ///
auto Uvec3::operator+(const Uvec3 &other) const -> Uvec3 { return {x + other.x, y + other.y, z + other.z}; }
auto Uvec3::operator+(uint32_t scalar) const -> Uvec3 { return {x + scalar, y + scalar, z + scalar}; }
auto Uvec3::operator+=(const Uvec3 &other) -> Uvec3 & {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}
auto Uvec3::operator+=(uint32_t scalar) -> Uvec3 & {
  x += scalar;
  y += scalar;
  z += scalar;
  return *this;
}

/// Subtraction ///
auto Uvec3::operator-(const Uvec3 &other) const -> Uvec3 { return {x - other.x, y - other.y, z - other.z}; }
auto Uvec3::operator-(uint32_t scalar) const -> Uvec3 { return {x - scalar, y - scalar, z - scalar}; }
auto Uvec3::operator-=(const Uvec3 &other) -> Uvec3 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}
auto Uvec3::operator-=(uint32_t scalar) -> Uvec3 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  return *this;
}

/// Multiplication ///
auto Uvec3::operator*(uint32_t scalar) const -> Uvec3 { return {x * scalar, y * scalar, z * scalar}; }
auto Uvec3::operator*(const Uvec3 &other) const -> Uvec3 { return {x * other.x, y * other.y, z * other.z}; }
auto Uvec3::operator*=(uint32_t scalar) -> Uvec3 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}
auto Uvec3::operator*=(const Uvec3 &other) -> Uvec3 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  return *this;
}

/// Division ///
auto Uvec3::operator/(uint32_t scalar) const -> Uvec3 { return {x / scalar, y / scalar, z / scalar}; }
auto Uvec3::operator/(const Uvec3 &other) const -> Uvec3 { return {x / other.x, y / other.y, z / other.z}; }
auto Uvec3::operator/=(uint32_t scalar) -> Uvec3 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  return *this;
}
auto Uvec3::operator/=(const Uvec3 &other) -> Uvec3 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  return *this;
}

auto Uvec3::operator==(const Uvec3 &other) const -> bool { return x == other.x && y == other.y && z == other.z; }
auto Uvec3::operator!=(const Uvec3 &other) const -> bool { return !(*this == other); }
Uvec3::Uvec3(const Uvec2 &vec2, uint32_t z_val) : x(vec2.x), y(vec2.y), z(z_val) {}
Uvec3::Uvec3(const Uvec4 &vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}
Uvec3::Uvec3(const Ivec2 &vec2, uint32_t z_val) : x(static_cast<uint32_t>(vec2.x)), y(static_cast<uint32_t>(vec2.y)), z(z_val) {}
Uvec3::Uvec3(const Ivec3 &vec3) : x(static_cast<uint32_t>(vec3.x)), y(static_cast<uint32_t>(vec3.y)), z(static_cast<uint32_t>(vec3.z)) {}
Uvec3::Uvec3(const Ivec4 &vec4) : x(static_cast<uint32_t>(vec4.x)), y(static_cast<uint32_t>(vec4.y)), z(static_cast<uint32_t>(vec4.z)) {}

///// Uvec4 /////

/// Addition ///
auto Uvec4::operator+(const Uvec4 &other) const -> Uvec4 { return {x + other.x, y + other.y, z + other.z, w + other.w}; }
auto Uvec4::operator+(uint32_t scalar) const -> Uvec4 { return {x + scalar, y + scalar, z + scalar, w + scalar}; }
auto Uvec4::operator+=(const Uvec4 &other) -> Uvec4 & {
  x += other.x;
  y += other.y;
  z += other.z;
  w += other.w;
  return *this;
}
auto Uvec4::operator+=(uint32_t scalar) -> Uvec4 & {
  x += scalar;
  y += scalar;
  z += scalar;
  w += scalar;
  return *this;
}

/// Subtraction ///
auto Uvec4::operator-(const Uvec4 &other) const -> Uvec4 { return {x - other.x, y - other.y, z - other.z, w - other.w}; }
auto Uvec4::operator-(uint32_t scalar) const -> Uvec4 { return {x - scalar, y - scalar, z - scalar, w - scalar}; }
auto Uvec4::operator-=(const Uvec4 &other) -> Uvec4 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  w -= other.w;
  return *this;
}
auto Uvec4::operator-=(uint32_t scalar) -> Uvec4 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  w -= scalar;
  return *this;
}

/// Multiplication ///
auto Uvec4::operator*(uint32_t scalar) const -> Uvec4 { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
auto Uvec4::operator*(const Uvec4 &other) const -> Uvec4 { return {x * other.x, y * other.y, z * other.z, w * other.w}; }
auto Uvec4::operator*=(uint32_t scalar) -> Uvec4 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
  return *this;
}
auto Uvec4::operator*=(const Uvec4 &other) -> Uvec4 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  w *= other.w;
  return *this;
}

/// Division ///
auto Uvec4::operator/(uint32_t scalar) const -> Uvec4 { return {x / scalar, y / scalar, z / scalar, w / scalar}; }
auto Uvec4::operator/(const Uvec4 &other) const -> Uvec4 { return {x / other.x, y / other.y, z / other.z, w / other.w}; }
auto Uvec4::operator/=(uint32_t scalar) -> Uvec4 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  w /= scalar;
  return *this;
}
auto Uvec4::operator/=(const Uvec4 &other) -> Uvec4 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  w /= other.w;
  return *this;
}

auto Uvec4::operator==(const Uvec4 &other) const -> bool { return x == other.x && y == other.y && z == other.z && w == other.w; }
auto Uvec4::operator!=(const Uvec4 &other) const -> bool { return !(*this == other); }
Uvec4::Uvec4(const Uvec2 &vec2, uint32_t z_val, uint32_t w_val) : x(vec2.x), y(vec2.y), z(z_val), w(w_val) {}
Uvec4::Uvec4(const Uvec3 &vec3, uint32_t w_val) : x(vec3.x), y(vec3.y), z(vec3.z), w(w_val) {}
Uvec4::Uvec4(const Ivec2 &vec2, uint32_t z_val, uint32_t w_val) : x(static_cast<uint32_t>(vec2.x)), y(static_cast<uint32_t>(vec2.y)), z(z_val), w(w_val) {}
Uvec4::Uvec4(const Ivec3 &vec3, uint32_t w_val)
    : x(static_cast<uint32_t>(vec3.x)), y(static_cast<uint32_t>(vec3.y)), z(static_cast<uint32_t>(vec3.z)), w(w_val) {}
Uvec4::Uvec4(const Ivec4 &vec4)
    : x(static_cast<uint32_t>(vec4.x)), y(static_cast<uint32_t>(vec4.y)), z(static_cast<uint32_t>(vec4.z)), w(static_cast<uint32_t>(vec4.w)) {}

///// Ivec2 /////

/// Addition ///
auto Ivec2::operator+(const Ivec2 &other) const -> Ivec2 { return {x + other.x, y + other.y}; }
auto Ivec2::operator+(int32_t scalar) const -> Ivec2 { return {x + scalar, y + scalar}; }
auto Ivec2::operator+=(const Ivec2 &other) -> Ivec2 & {
  x += other.x;
  y += other.y;
  return *this;
}
auto Ivec2::operator+=(int32_t scalar) -> Ivec2 & {
  x += scalar;
  y += scalar;
  return *this;
}

/// Subtraction ///
auto Ivec2::operator-(const Ivec2 &other) const -> Ivec2 { return {x - other.x, y - other.y}; }
auto Ivec2::operator-(int32_t scalar) const -> Ivec2 { return {x - scalar, y - scalar}; }
auto Ivec2::operator-=(const Ivec2 &other) -> Ivec2 & {
  x -= other.x;
  y -= other.y;
  return *this;
}
auto Ivec2::operator-=(int32_t scalar) -> Ivec2 & {
  x -= scalar;
  y -= scalar;
  return *this;
}

/// Multiplication ///
auto Ivec2::operator*(int32_t scalar) const -> Ivec2 { return {x * scalar, y * scalar}; }
auto Ivec2::operator*(const Ivec2 &other) const -> Ivec2 { return {x * other.x, y * other.y}; }
auto Ivec2::operator*=(int32_t scalar) -> Ivec2 & {
  x *= scalar;
  y *= scalar;
  return *this;
}
auto Ivec2::operator*=(const Ivec2 &other) -> Ivec2 & {
  x *= other.x;
  y *= other.y;
  return *this;
}

/// Division ///
auto Ivec2::operator/(int32_t scalar) const -> Ivec2 { return {x / scalar, y / scalar}; }
auto Ivec2::operator/(const Ivec2 &other) const -> Ivec2 { return {x / other.x, y / other.y}; }
auto Ivec2::operator/=(int32_t scalar) -> Ivec2 & {
  x /= scalar;
  y /= scalar;
  return *this;
}
auto Ivec2::operator/=(const Ivec2 &other) -> Ivec2 & {
  x /= other.x;
  y /= other.y;
  return *this;
}

auto Ivec2::operator==(const Ivec2 &other) const -> bool { return x == other.x && y == other.y; }
auto Ivec2::operator!=(const Ivec2 &other) const -> bool { return !(*this == other); }
Ivec2::Ivec2(const Ivec3 &vec3) : x(vec3.x), y(vec3.y) {}
Ivec2::Ivec2(const Ivec4 &vec4) : x(vec4.x), y(vec4.y) {}
Ivec2::Ivec2(const Uvec2 &vec2) : x(static_cast<int32_t>(vec2.x)), y(static_cast<int32_t>(vec2.y)) {}
Ivec2::Ivec2(const Uvec3 &vec3) : x(static_cast<int32_t>(vec3.x)), y(static_cast<int32_t>(vec3.y)) {}
Ivec2::Ivec2(const Uvec4 &vec4) : x(static_cast<int32_t>(vec4.x)), y(static_cast<int32_t>(vec4.y)) {}

///// Ivec3 /////

/// Addition ///
auto Ivec3::operator+(const Ivec3 &other) const -> Ivec3 { return {x + other.x, y + other.y, z + other.z}; }
auto Ivec3::operator+(int32_t scalar) const -> Ivec3 { return {x + scalar, y + scalar, z + scalar}; }
auto Ivec3::operator+=(const Ivec3 &other) -> Ivec3 & {
  x += other.x;
  y += other.y;
  z += other.z;
  return *this;
}
auto Ivec3::operator+=(int32_t scalar) -> Ivec3 & {
  x += scalar;
  y += scalar;
  z += scalar;
  return *this;
}

/// Subtraction ///
auto Ivec3::operator-(const Ivec3 &other) const -> Ivec3 { return {x - other.x, y - other.y, z - other.z}; }
auto Ivec3::operator-(int32_t scalar) const -> Ivec3 { return {x - scalar, y - scalar, z - scalar}; }
auto Ivec3::operator-=(const Ivec3 &other) -> Ivec3 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  return *this;
}
auto Ivec3::operator-=(int32_t scalar) -> Ivec3 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  return *this;
}

/// Multiplication ///
auto Ivec3::operator*(int32_t scalar) const -> Ivec3 { return {x * scalar, y * scalar, z * scalar}; }
auto Ivec3::operator*(const Ivec3 &other) const -> Ivec3 { return {x * other.x, y * other.y, z * other.z}; }
auto Ivec3::operator*=(int32_t scalar) -> Ivec3 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  return *this;
}
auto Ivec3::operator*=(const Ivec3 &other) -> Ivec3 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  return *this;
}

/// Division ///
auto Ivec3::operator/(int32_t scalar) const -> Ivec3 { return {x / scalar, y / scalar, z / scalar}; }
auto Ivec3::operator/(const Ivec3 &other) const -> Ivec3 { return {x / other.x, y / other.y, z / other.z}; }
auto Ivec3::operator/=(int32_t scalar) -> Ivec3 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  return *this;
}
auto Ivec3::operator/=(const Ivec3 &other) -> Ivec3 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  return *this;
}

auto Ivec3::operator==(const Ivec3 &other) const -> bool { return x == other.x && y == other.y && z == other.z; }
auto Ivec3::operator!=(const Ivec3 &other) const -> bool { return !(*this == other); }
Ivec3::Ivec3(const Ivec2 &vec2, int32_t z_val) : x(vec2.x), y(vec2.y), z(z_val) {}
Ivec3::Ivec3(const Ivec4 &vec4) : x(vec4.x), y(vec4.y), z(vec4.z) {}
Ivec3::Ivec3(const Uvec2 &vec2, int32_t z_val) : x(static_cast<int32_t>(vec2.x)), y(static_cast<int32_t>(vec2.y)), z(z_val) {}
Ivec3::Ivec3(const Uvec3 &vec3) : x(static_cast<int32_t>(vec3.x)), y(static_cast<int32_t>(vec3.y)), z(static_cast<int32_t>(vec3.z)) {}
Ivec3::Ivec3(const Uvec4 &vec4) : x(static_cast<int32_t>(vec4.x)), y(static_cast<int32_t>(vec4.y)), z(static_cast<int32_t>(vec4.z)) {}

///// Ivec4 /////

/// Addition ///
auto Ivec4::operator+(const Ivec4 &other) const -> Ivec4 { return {x + other.x, y + other.y, z + other.z, w + other.w}; }
auto Ivec4::operator+(int32_t scalar) const -> Ivec4 { return {x + scalar, y + scalar, z + scalar, w + scalar}; }
auto Ivec4::operator+=(const Ivec4 &other) -> Ivec4 & {
  x += other.x;
  y += other.y;
  z += other.z;
  w += other.w;
  return *this;
}
auto Ivec4::operator+=(int32_t scalar) -> Ivec4 & {
  x += scalar;
  y += scalar;
  z += scalar;
  w += scalar;
  return *this;
}

/// Subtraction ///
auto Ivec4::operator-(const Ivec4 &other) const -> Ivec4 { return {x - other.x, y - other.y, z - other.z, w - other.w}; }
auto Ivec4::operator-(int32_t scalar) const -> Ivec4 { return {x - scalar, y - scalar, z - scalar, w - scalar}; }
auto Ivec4::operator-=(const Ivec4 &other) -> Ivec4 & {
  x -= other.x;
  y -= other.y;
  z -= other.z;
  w -= other.w;
  return *this;
}
auto Ivec4::operator-=(int32_t scalar) -> Ivec4 & {
  x -= scalar;
  y -= scalar;
  z -= scalar;
  w -= scalar;
  return *this;
}

/// Multiplication ///
auto Ivec4::operator*(int32_t scalar) const -> Ivec4 { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
auto Ivec4::operator*(const Ivec4 &other) const -> Ivec4 { return {x * other.x, y * other.y, z * other.z, w * other.w}; }
auto Ivec4::operator*=(int32_t scalar) -> Ivec4 & {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
  return *this;
}
auto Ivec4::operator*=(const Ivec4 &other) -> Ivec4 & {
  x *= other.x;
  y *= other.y;
  z *= other.z;
  w *= other.w;
  return *this;
}

/// Division ///
auto Ivec4::operator/(int32_t scalar) const -> Ivec4 { return {x / scalar, y / scalar, z / scalar, w / scalar}; }
auto Ivec4::operator/(const Ivec4 &other) const -> Ivec4 { return {x / other.x, y / other.y, z / other.z, w / other.w}; }
auto Ivec4::operator/=(int32_t scalar) -> Ivec4 & {
  x /= scalar;
  y /= scalar;
  z /= scalar;
  w /= scalar;
  return *this;
}
auto Ivec4::operator/=(const Ivec4 &other) -> Ivec4 & {
  x /= other.x;
  y /= other.y;
  z /= other.z;
  w /= other.w;
  return *this;
}

auto Ivec4::operator==(const Ivec4 &other) const -> bool { return x == other.x && y == other.y && z == other.z && w == other.w; }
auto Ivec4::operator!=(const Ivec4 &other) const -> bool { return !(*this == other); }
Ivec4::Ivec4(const Ivec2 &vec2, int32_t z_val, int32_t w_val) : x(vec2.x), y(vec2.y), z(z_val), w(w_val) {}
Ivec4::Ivec4(const Ivec3 &vec3, int32_t w_val) : x(vec3.x), y(vec3.y), z(vec3.z), w(w_val) {}
Ivec4::Ivec4(const Uvec2 &vec2, int32_t z_val, int32_t w_val) : x(static_cast<int32_t>(vec2.x)), y(static_cast<int32_t>(vec2.y)), z(z_val), w(w_val) {}
Ivec4::Ivec4(const Uvec3 &vec3, int32_t w_val) : x(static_cast<int32_t>(vec3.x)), y(static_cast<int32_t>(vec3.y)), z(static_cast<int32_t>(vec3.z)), w(w_val) {}
Ivec4::Ivec4(const Uvec4 &vec4)
    : x(static_cast<int32_t>(vec4.x)), y(static_cast<int32_t>(vec4.y)), z(static_cast<int32_t>(vec4.z)), w(static_cast<int32_t>(vec4.w)) {}

auto Min(const Vec2 &vec_a, const Vec2 &vec_b) -> Vec2 { return {std::fmin(vec_a.x, vec_b.x), std::fmin(vec_a.y, vec_b.y)}; }
auto Min(const Vec3 &vec_a, const Vec3 &vec_b) -> Vec3 { return {std::fmin(vec_a.x, vec_b.x), std::fmin(vec_a.y, vec_b.y), std::fmin(vec_a.z, vec_b.z)}; }
auto Min(const Vec4 &vec_a, const Vec4 &vec_b) -> Vec4 {
  return {std::fmin(vec_a.x, vec_b.x), std::fmin(vec_a.y, vec_b.y), std::fmin(vec_a.z, vec_b.z), std::fmin(vec_a.w, vec_b.w)};
}

auto Max(const Vec2 &vec_a, const Vec2 &vec_b) -> Vec2 { return {std::fmax(vec_a.x, vec_b.x), std::fmax(vec_a.y, vec_b.y)}; }
auto Max(const Vec3 &vec_a, const Vec3 &vec_b) -> Vec3 { return {std::fmax(vec_a.x, vec_b.x), std::fmax(vec_a.y, vec_b.y), std::fmax(vec_a.z, vec_b.z)}; }
auto Max(const Vec4 &vec_a, const Vec4 &vec_b) -> Vec4 {
  return {std::fmax(vec_a.x, vec_b.x), std::fmax(vec_a.y, vec_b.y), std::fmax(vec_a.z, vec_b.z), std::fmax(vec_a.w, vec_b.w)};
}

auto Min(const Uvec2 &vec_a, const Uvec2 &vec_b) -> Uvec2 { return {std::min(vec_a.x, vec_b.x), std::min(vec_a.y, vec_b.y)}; }
auto Min(const Uvec3 &vec_a, const Uvec3 &vec_b) -> Uvec3 { return {std::min(vec_a.x, vec_b.x), std::min(vec_a.y, vec_b.y), std::min(vec_a.z, vec_b.z)}; }
auto Min(const Uvec4 &vec_a, const Uvec4 &vec_b) -> Uvec4 {
  return {std::min(vec_a.x, vec_b.x), std::min(vec_a.y, vec_b.y), std::min(vec_a.z, vec_b.z), std::min(vec_a.w, vec_b.w)};
}

auto Max(const Uvec2 &vec_a, const Uvec2 &vec_b) -> Uvec2 { return {std::max(vec_a.x, vec_b.x), std::max(vec_a.y, vec_b.y)}; }
auto Max(const Uvec3 &vec_a, const Uvec3 &vec_b) -> Uvec3 { return {std::max(vec_a.x, vec_b.x), std::max(vec_a.y, vec_b.y), std::max(vec_a.z, vec_b.z)}; }
auto Max(const Uvec4 &vec_a, const Uvec4 &vec_b) -> Uvec4 {
  return {std::max(vec_a.x, vec_b.x), std::max(vec_a.y, vec_b.y), std::max(vec_a.z, vec_b.z), std::max(vec_a.w, vec_b.w)};
}

auto Min(const Ivec2 &vec_a, const Ivec2 &vec_b) -> Ivec2 { return {std::min(vec_a.x, vec_b.x), std::min(vec_a.y, vec_b.y)}; }
auto Min(const Ivec3 &vec_a, const Ivec3 &vec_b) -> Ivec3 { return {std::min(vec_a.x, vec_b.x), std::min(vec_a.y, vec_b.y), std::min(vec_a.z, vec_b.z)}; }
auto Min(const Ivec4 &vec_a, const Ivec4 &vec_b) -> Ivec4 {
  return {std::min(vec_a.x, vec_b.x), std::min(vec_a.y, vec_b.y), std::min(vec_a.z, vec_b.z), std::min(vec_a.w, vec_b.w)};
}

auto Max(const Ivec2 &vec_a, const Ivec2 &vec_b) -> Ivec2 { return {std::max(vec_a.x, vec_b.x), std::max(vec_a.y, vec_b.y)}; }
auto Max(const Ivec3 &vec_a, const Ivec3 &vec_b) -> Ivec3 { return {std::max(vec_a.x, vec_b.x), std::max(vec_a.y, vec_b.y), std::max(vec_a.z, vec_b.z)}; }
auto Max(const Ivec4 &vec_a, const Ivec4 &vec_b) -> Ivec4 {
  return {std::max(vec_a.x, vec_b.x), std::max(vec_a.y, vec_b.y), std::max(vec_a.z, vec_b.z), std::max(vec_a.w, vec_b.w)};
}

/*
struct Plane : public Vec4 {
  [[nodiscard]] auto Normal() const -> Vec3;
  [[nodiscard]] auto Distance() const -> float;
  [[nodiscard]] auto Normalize() const -> Plane;
  [[nodiscard]] auto Point() const -> Vec3;
  [[nodiscard]] auto DistanceToPoint(const Vec3 &point) const -> float;
  [[nodiscard]] auto IntersectRay(const Vec3 &origin, const Vec3 &direction) const
      -> std::optional<Vec3>;
};
*/

auto Plane::Normal() const -> Vec3 { return {x, y, z}; }

auto Plane::Distance() const -> float { return w; }

auto Plane::Normalize() const -> Plane {
  float length = Normal().Length();
  if (length <= Epsilon) {
    return {0.0F, 0.0F, 0.0F, 0.0F};
  }
  float inv_length = 1.0F / length;
  return {x * inv_length, y * inv_length, z * inv_length, w * inv_length};
}

auto Plane::Point() const -> Vec3 {
  Vec3 normal = Normal();
  float distance = Distance();
  return normal * distance;
}

auto Plane::DistanceToPoint(const Vec3 &point) const -> float {
  Vec3 normal = Normal();
  float distance = Distance();
  return normal.Dot(point) - distance;
}

auto Plane::IntersectRay(const Vec3 &origin, const Vec3 &direction) const -> std::optional<Vec3> {
  Vec3 normal = Normal();
  float distance = Distance();
  float denom = normal.Dot(direction);
  if (std::abs(denom) < Epsilon) {
    return std::nullopt; // Ray is parallel to the plane
  }
  float time = (distance - normal.Dot(origin)) / denom;
  if (time < 0) {
    return std::nullopt; // Intersection is behind the ray origin
  }
  return origin + direction * time;
}

} // namespace Math