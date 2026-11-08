#pragma once

#include "mathTypes.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace Math {

struct Vec2;
struct Vec3;
struct Vec4;

struct Uvec2;
struct Uvec3;
struct Uvec4;

struct Ivec2;
struct Ivec3;
struct Ivec4;

struct Vec2 {
  float x{};
  float y{};

  auto operator+(const Vec2 &other) const -> Vec2;
  auto operator+(float scalar) const -> Vec2;
  auto operator+=(const Vec2 &other) -> Vec2 &;
  auto operator+=(float scalar) -> Vec2 &;

  auto operator-(const Vec2 &other) const -> Vec2;
  auto operator-(float scalar) const -> Vec2;
  auto operator-=(const Vec2 &other) -> Vec2 &;
  auto operator-=(float scalar) -> Vec2 &;

  auto operator-() const -> Vec2;

  auto operator*(float scalar) const -> Vec2;
  auto operator*(const Vec2 &other) const -> Vec2;
  auto operator*=(float scalar) -> Vec2 &;
  auto operator*=(const Vec2 &other) -> Vec2 &;

  auto operator/(float scalar) const -> Vec2;
  auto operator/(const Vec2 &other) const -> Vec2;
  auto operator/=(float scalar) -> Vec2 &;
  auto operator/=(const Vec2 &other) -> Vec2 &;

  auto operator==(const Vec2 &other) const -> bool;
  auto operator!=(const Vec2 &other) const -> bool;

  auto operator[](uint32_t index) -> float &;
  auto operator[](uint32_t index) const -> float;

  [[nodiscard]] auto Length() const -> float;
  [[nodiscard]] auto LengthSqr() const -> float;
  [[nodiscard]] auto Inverse() const -> Vec2;
  [[nodiscard]] auto Normalize() const -> Vec2;
  [[nodiscard]] auto Distance(const Vec2 &other) const -> float;
  [[nodiscard]] auto DistanceSqr(const Vec2 &other) const -> float;
  [[nodiscard]] auto Dot(const Vec2 &other) const -> float;
  [[nodiscard]] auto Cross(const Vec2 &other) const -> Vec2;

  [[nodiscard]] auto Max(const Vec2 &other) const -> Vec2;
  [[nodiscard]] auto Max(float scalar) const -> Vec2;
  [[nodiscard]] auto Min(const Vec2 &other) const -> Vec2;
  [[nodiscard]] auto Min(float scalar) const -> Vec2;

  constexpr Vec2(float x_val, float y_val) : x(x_val), y(y_val) {}
  constexpr explicit Vec2(float val) : x(val), y(val) {}
  constexpr Vec2() = default;
  explicit Vec2(const Vec3 &vec3);
  explicit Vec2(const Vec4 &vec4);

  explicit Vec2(const Ivec2 &vec2);
  explicit Vec2(const Ivec3 &vec3);
  explicit Vec2(const Ivec4 &vec4);

  explicit Vec2(const Uvec2 &vec2);
  explicit Vec2(const Uvec3 &vec3);
  explicit Vec2(const Uvec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<float> { return {&x, 2}; }
  [[nodiscard]] auto Span() const -> std::span<const float> { return {&x, 2}; }
  [[nodiscard]] auto Ptr() -> float * { return &x; }
  [[nodiscard]] auto Ptr() const -> const float * { return &x; }
};

struct Vec3 {
  float x{};
  float y{};
  float z{};

  auto operator+(const Vec3 &other) const -> Vec3;
  auto operator+(float scalar) const -> Vec3;
  auto operator+=(const Vec3 &other) -> Vec3 &;
  auto operator+=(float scalar) -> Vec3 &;

  auto operator-(const Vec3 &other) const -> Vec3;
  auto operator-(float scalar) const -> Vec3;
  auto operator-=(const Vec3 &other) -> Vec3 &;
  auto operator-=(float scalar) -> Vec3 &;

  auto operator-() const -> Vec3;

  auto operator*(float scalar) const -> Vec3;
  auto operator*(const Vec3 &other) const -> Vec3;
  auto operator*=(float scalar) -> Vec3 &;
  auto operator*=(const Vec3 &other) -> Vec3 &;

  auto operator/(float scalar) const -> Vec3;
  auto operator/(const Vec3 &other) const -> Vec3;
  auto operator/=(float scalar) -> Vec3 &;
  auto operator/=(const Vec3 &other) -> Vec3 &;

  auto operator==(const Vec3 &other) const -> bool;
  auto operator!=(const Vec3 &other) const -> bool;

  auto operator[](uint32_t index) -> float &;
  auto operator[](uint32_t index) const -> float;

  [[nodiscard]] auto Length() const -> float;
  [[nodiscard]] auto LengthSqr() const -> float;
  [[nodiscard]] auto Inverse() const -> Vec3;
  [[nodiscard]] auto Normalize() const -> Vec3;
  [[nodiscard]] auto Distance(const Vec3 &other) const -> float;
  [[nodiscard]] auto DistanceSqr(const Vec3 &other) const -> float;
  [[nodiscard]] auto Dot(const Vec3 &other) const -> float;
  [[nodiscard]] auto Cross(const Vec3 &other) const -> Vec3;
  [[nodiscard]] auto GetTangent() const -> Vec3;

  [[nodiscard]] auto Max(const Vec3 &other) const -> Vec3;
  [[nodiscard]] auto Max(float scalar) const -> Vec3;
  [[nodiscard]] auto Min(const Vec3 &other) const -> Vec3;
  [[nodiscard]] auto Min(float scalar) const -> Vec3;

  constexpr Vec3(float x_val, float y_val, float z_val) : x(x_val), y(y_val), z(z_val) {}
  constexpr explicit Vec3(float val) : x(val), y(val), z(val) {}
  constexpr Vec3() = default;
  explicit Vec3(const Vec2 &vec2, float z_val = 0.0F);
  explicit Vec3(const Vec4 &vec4);

  explicit Vec3(const Ivec2 &vec2, float z_val = 0.0F);
  explicit Vec3(const Ivec3 &vec3);
  explicit Vec3(const Ivec4 &vec4);

  explicit Vec3(const Uvec2 &vec2, float z_val = 0.0F);
  explicit Vec3(const Uvec3 &vec3);
  explicit Vec3(const Uvec4 &vec4);

  explicit Vec3(const Vec4 &&vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<float> { return {&x, 3}; }
  [[nodiscard]] auto Span() const -> std::span<const float> { return {&x, 3}; }
  [[nodiscard]] auto Ptr() -> float * { return &x; }
  [[nodiscard]] auto Ptr() const -> const float * { return &x; }
};

struct Vec4 {
  float x{};
  float y{};
  float z{};
  float w{};

  auto operator+(const Vec4 &other) const -> Vec4;
  auto operator+(float scalar) const -> Vec4;
  auto operator+=(const Vec4 &other) -> Vec4 &;
  auto operator+=(float scalar) -> Vec4 &;

  auto operator-(const Vec4 &other) const -> Vec4;
  auto operator-(float scalar) const -> Vec4;
  auto operator-=(const Vec4 &other) -> Vec4 &;
  auto operator-=(float scalar) -> Vec4 &;

  auto operator-() const -> Vec4;

  auto operator*(float scalar) const -> Vec4;
  auto operator*(const Vec4 &other) const -> Vec4;
  auto operator*=(float scalar) -> Vec4 &;
  auto operator*=(const Vec4 &other) -> Vec4 &;

  auto operator/(float scalar) const -> Vec4;
  auto operator/(const Vec4 &other) const -> Vec4;
  auto operator/=(float scalar) -> Vec4 &;
  auto operator/=(const Vec4 &other) -> Vec4 &;

  auto operator==(const Vec4 &other) const -> bool;
  auto operator!=(const Vec4 &other) const -> bool;

  auto operator[](uint32_t index) -> float &;
  auto operator[](uint32_t index) const -> float;

  [[nodiscard]] auto Length() const -> float;
  [[nodiscard]] auto LengthSqr() const -> float;
  [[nodiscard]] auto Inverse() const -> Vec4;
  [[nodiscard]] auto Normalize() const -> Vec4;
  [[nodiscard]] auto Distance(const Vec4 &other) const -> float;
  [[nodiscard]] auto DistanceSqr(const Vec4 &other) const -> float;
  [[nodiscard]] auto Dot(const Vec4 &other) const -> float;
  [[nodiscard]] auto Dot(const Vec3 &other) const -> float;
  [[nodiscard]] auto Cross(const Vec4 &other) const -> Vec3;

  [[nodiscard]] auto Max(const Vec4 &other) const -> Vec4;
  [[nodiscard]] auto Max(float scalar) const -> Vec4;
  [[nodiscard]] auto Min(const Vec4 &other) const -> Vec4;
  [[nodiscard]] auto Min(float scalar) const -> Vec4;

  constexpr Vec4(float x_val, float y_val, float z_val, float w_val) : x(x_val), y(y_val), z(z_val), w(w_val) {}
  constexpr explicit Vec4(float val) : x(val), y(val), z(val), w(val) {}
  constexpr Vec4() = default;
  explicit Vec4(const Vec2 &vec2, float z_val = 0.0F, float w_val = 0.0F);
  explicit Vec4(const Vec3 &vec3, float w_val = 0.0F);

  explicit Vec4(const Ivec2 &vec2, float z_val = 0.0F, float w_val = 0.0F);
  explicit Vec4(const Ivec3 &vec3, float w_val = 0.0F);
  explicit Vec4(const Ivec4 &vec4);

  explicit Vec4(const Uvec2 &vec2, float z_val = 0.0F, float w_val = 0.0F);
  explicit Vec4(const Uvec3 &vec3, float w_val = 0.0F);
  explicit Vec4(const Uvec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<float> { return {&x, 4}; }
  [[nodiscard]] auto Span() const -> std::span<const float> { return {&x, 4}; }
  [[nodiscard]] auto Ptr() -> float * { return &x; }
  [[nodiscard]] auto Ptr() const -> const float * { return &x; }
};

struct Uvec2 {
  uint32_t x;
  uint32_t y;

  auto operator+(const Uvec2 &other) const -> Uvec2;
  auto operator+(uint32_t scalar) const -> Uvec2;
  auto operator+=(const Uvec2 &other) -> Uvec2 &;
  auto operator+=(uint32_t scalar) -> Uvec2 &;

  auto operator-(const Uvec2 &other) const -> Uvec2;
  auto operator-(uint32_t scalar) const -> Uvec2;
  auto operator-=(const Uvec2 &other) -> Uvec2 &;
  auto operator-=(uint32_t scalar) -> Uvec2 &;

  auto operator*(uint32_t scalar) const -> Uvec2;
  auto operator*(const Uvec2 &other) const -> Uvec2;
  auto operator*=(uint32_t scalar) -> Uvec2 &;
  auto operator*=(const Uvec2 &other) -> Uvec2 &;

  auto operator/(uint32_t scalar) const -> Uvec2;
  auto operator/(const Uvec2 &other) const -> Uvec2;
  auto operator/=(uint32_t scalar) -> Uvec2 &;
  auto operator/=(const Uvec2 &other) -> Uvec2 &;

  auto operator%(uint32_t scalar) const -> Uvec2;
  auto operator%(const Uvec2 &other) const -> Uvec2;
  auto operator%=(uint32_t scalar) -> Uvec2 &;
  auto operator%=(const Uvec2 &other) -> Uvec2 &;

  auto operator==(const Uvec2 &other) const -> bool;
  auto operator!=(const Uvec2 &other) const -> bool;

  auto operator[](uint32_t index) const -> uint32_t;
  auto operator[](uint32_t index) -> uint32_t &;

  constexpr Uvec2(uint32_t x_val, uint32_t y_val) : x(x_val), y(y_val) {}
  constexpr explicit Uvec2(uint32_t val) : x(val), y(val) {}
  constexpr Uvec2() = default;
  explicit Uvec2(const Uvec3 &vec3);
  explicit Uvec2(const Uvec4 &vec4);

  explicit Uvec2(const Ivec2 &vec2);
  explicit Uvec2(const Ivec3 &vec3);
  explicit Uvec2(const Ivec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<uint32_t> { return {&x, 2}; }
  [[nodiscard]] auto Span() const -> std::span<const uint32_t> { return {&x, 2}; }
  [[nodiscard]] auto Ptr() -> uint32_t * { return &x; }
  [[nodiscard]] auto Ptr() const -> const uint32_t * { return &x; }
};

struct Uvec3 {
  uint32_t x;
  uint32_t y;
  uint32_t z;

  auto operator+(const Uvec3 &other) const -> Uvec3;
  auto operator+(uint32_t scalar) const -> Uvec3;
  auto operator+=(const Uvec3 &other) -> Uvec3 &;
  auto operator+=(uint32_t scalar) -> Uvec3 &;

  auto operator-(const Uvec3 &other) const -> Uvec3;
  auto operator-(uint32_t scalar) const -> Uvec3;
  auto operator-=(const Uvec3 &other) -> Uvec3 &;
  auto operator-=(uint32_t scalar) -> Uvec3 &;

  auto operator*(uint32_t scalar) const -> Uvec3;
  auto operator*(const Uvec3 &other) const -> Uvec3;
  auto operator*=(uint32_t scalar) -> Uvec3 &;
  auto operator*=(const Uvec3 &other) -> Uvec3 &;

  auto operator/(uint32_t scalar) const -> Uvec3;
  auto operator/(const Uvec3 &other) const -> Uvec3;
  auto operator/=(uint32_t scalar) -> Uvec3 &;
  auto operator/=(const Uvec3 &other) -> Uvec3 &;

  auto operator==(const Uvec3 &other) const -> bool;
  auto operator!=(const Uvec3 &other) const -> bool;

  auto operator[](uint32_t index) const -> uint32_t;
  auto operator[](uint32_t index) -> uint32_t &;

  constexpr Uvec3(uint32_t x_val, uint32_t y_val, uint32_t z_val) : x(x_val), y(y_val), z(z_val) {}
  constexpr explicit Uvec3(uint32_t val) : x(val), y(val), z(val) {}
  constexpr Uvec3() = default;
  explicit Uvec3(const Uvec2 &vec2, uint32_t z_val = 0);
  explicit Uvec3(const Uvec4 &vec4);

  explicit Uvec3(const Ivec2 &vec2, uint32_t z_val = 0);
  explicit Uvec3(const Ivec3 &vec3);
  explicit Uvec3(const Ivec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<uint32_t> { return {&x, 3}; }
  [[nodiscard]] auto Span() const -> std::span<const uint32_t> { return {&x, 3}; }
  [[nodiscard]] auto Ptr() -> uint32_t * { return &x; }
  [[nodiscard]] auto Ptr() const -> const uint32_t * { return &x; }
};

struct Uvec4 {
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t w;

  auto operator+(const Uvec4 &other) const -> Uvec4;
  auto operator+(uint32_t scalar) const -> Uvec4;
  auto operator+=(const Uvec4 &other) -> Uvec4 &;
  auto operator+=(uint32_t scalar) -> Uvec4 &;

  auto operator-(const Uvec4 &other) const -> Uvec4;
  auto operator-(uint32_t scalar) const -> Uvec4;
  auto operator-=(const Uvec4 &other) -> Uvec4 &;
  auto operator-=(uint32_t scalar) -> Uvec4 &;

  auto operator*(uint32_t scalar) const -> Uvec4;
  auto operator*(const Uvec4 &other) const -> Uvec4;
  auto operator*=(uint32_t scalar) -> Uvec4 &;
  auto operator*=(const Uvec4 &other) -> Uvec4 &;

  auto operator/(uint32_t scalar) const -> Uvec4;
  auto operator/(const Uvec4 &other) const -> Uvec4;
  auto operator/=(uint32_t scalar) -> Uvec4 &;
  auto operator/=(const Uvec4 &other) -> Uvec4 &;

  auto operator==(const Uvec4 &other) const -> bool;
  auto operator!=(const Uvec4 &other) const -> bool;

  auto operator[](uint32_t index) const -> uint32_t;
  auto operator[](uint32_t index) -> uint32_t &;

  constexpr Uvec4(uint32_t x_val, uint32_t y_val, uint32_t z_val, uint32_t w_val) : x(x_val), y(y_val), z(z_val), w(w_val) {}
  constexpr explicit Uvec4(uint32_t val) : x(val), y(val), z(val), w(val) {}
  constexpr Uvec4() = default;
  explicit Uvec4(const Uvec2 &vec2, uint32_t z_val = 0, uint32_t w_val = 0);
  explicit Uvec4(const Uvec3 &vec3, uint32_t w_val = 0);

  explicit Uvec4(const Ivec2 &vec2, uint32_t z_val = 0, uint32_t w_val = 0);
  explicit Uvec4(const Ivec3 &vec3, uint32_t w_val = 0);
  explicit Uvec4(const Ivec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<uint32_t> { return {&x, 4}; }
  [[nodiscard]] auto Span() const -> std::span<const uint32_t> { return {&x, 4}; }
  [[nodiscard]] auto Ptr() -> uint32_t * { return &x; }
  [[nodiscard]] auto Ptr() const -> const uint32_t * { return &x; }
};

struct Ivec2 {
  int32_t x;
  int32_t y;

  auto operator+(const Ivec2 &other) const -> Ivec2;
  auto operator+(int32_t scalar) const -> Ivec2;
  auto operator+=(const Ivec2 &other) -> Ivec2 &;
  auto operator+=(int32_t scalar) -> Ivec2 &;

  auto operator-(const Ivec2 &other) const -> Ivec2;
  auto operator-(int32_t scalar) const -> Ivec2;
  auto operator-=(const Ivec2 &other) -> Ivec2 &;
  auto operator-=(int32_t scalar) -> Ivec2 &;

  auto operator*(int32_t scalar) const -> Ivec2;
  auto operator*(const Ivec2 &other) const -> Ivec2;
  auto operator*=(int32_t scalar) -> Ivec2 &;
  auto operator*=(const Ivec2 &other) -> Ivec2 &;

  auto operator/(int32_t scalar) const -> Ivec2;
  auto operator/(const Ivec2 &other) const -> Ivec2;
  auto operator/=(int32_t scalar) -> Ivec2 &;
  auto operator/=(const Ivec2 &other) -> Ivec2 &;

  auto operator==(const Ivec2 &other) const -> bool;
  auto operator!=(const Ivec2 &other) const -> bool;

  auto operator[](uint32_t index) const -> int32_t;
  auto operator[](uint32_t index) -> int32_t &;

  constexpr Ivec2(int32_t x_val, int32_t y_val) : x(x_val), y(y_val) {}
  constexpr explicit Ivec2(int32_t val) : x(val), y(val) {}
  constexpr Ivec2() = default;
  explicit Ivec2(const Ivec3 &vec3);
  explicit Ivec2(const Ivec4 &vec4);

  explicit Ivec2(const Uvec2 &vec2);
  explicit Ivec2(const Uvec3 &vec3);
  explicit Ivec2(const Uvec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<int32_t> { return {&x, 2}; }
  [[nodiscard]] auto Span() const -> std::span<const int32_t> { return {&x, 2}; }
  [[nodiscard]] auto Ptr() -> int32_t * { return &x; }
  [[nodiscard]] auto Ptr() const -> const int32_t * { return &x; }
};

struct Ivec3 {
  int32_t x;
  int32_t y;
  int32_t z;

  auto operator+(const Ivec3 &other) const -> Ivec3;
  auto operator+(int32_t scalar) const -> Ivec3;
  auto operator+=(const Ivec3 &other) -> Ivec3 &;
  auto operator+=(int32_t scalar) -> Ivec3 &;

  auto operator-(const Ivec3 &other) const -> Ivec3;
  auto operator-(int32_t scalar) const -> Ivec3;
  auto operator-=(const Ivec3 &other) -> Ivec3 &;
  auto operator-=(int32_t scalar) -> Ivec3 &;

  auto operator*(int32_t scalar) const -> Ivec3;
  auto operator*(const Ivec3 &other) const -> Ivec3;
  auto operator*=(int32_t scalar) -> Ivec3 &;
  auto operator*=(const Ivec3 &other) -> Ivec3 &;

  auto operator/(int32_t scalar) const -> Ivec3;
  auto operator/(const Ivec3 &other) const -> Ivec3;
  auto operator/=(int32_t scalar) -> Ivec3 &;
  auto operator/=(const Ivec3 &other) -> Ivec3 &;

  auto operator==(const Ivec3 &other) const -> bool;
  auto operator!=(const Ivec3 &other) const -> bool;

  auto operator[](uint32_t index) const -> int32_t;
  auto operator[](uint32_t index) -> int32_t &;

  constexpr Ivec3(int32_t x_val, int32_t y_val, int32_t z_val) : x(x_val), y(y_val), z(z_val) {}
  constexpr explicit Ivec3(int32_t val) : x(val), y(val), z(val) {}
  constexpr Ivec3() = default;
  explicit Ivec3(const Ivec2 &vec2, int32_t z_val = 0);
  explicit Ivec3(const Ivec4 &vec4);

  explicit Ivec3(const Uvec2 &vec2, int32_t z_val = 0);
  explicit Ivec3(const Uvec3 &vec3);
  explicit Ivec3(const Uvec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<int32_t> { return {&x, 3}; }
  [[nodiscard]] auto Span() const -> std::span<const int32_t> { return {&x, 3}; }
  [[nodiscard]] auto Ptr() -> int32_t * { return &x; }
  [[nodiscard]] auto Ptr() const -> const int32_t * { return &x; }
};

struct Ivec4 {
  int32_t x;
  int32_t y;
  int32_t z;
  int32_t w;

  auto operator+(const Ivec4 &other) const -> Ivec4;
  auto operator+(int32_t scalar) const -> Ivec4;
  auto operator+=(const Ivec4 &other) -> Ivec4 &;
  auto operator+=(int32_t scalar) -> Ivec4 &;

  auto operator-(const Ivec4 &other) const -> Ivec4;
  auto operator-(int32_t scalar) const -> Ivec4;
  auto operator-=(const Ivec4 &other) -> Ivec4 &;
  auto operator-=(int32_t scalar) -> Ivec4 &;

  auto operator*(int32_t scalar) const -> Ivec4;
  auto operator*(const Ivec4 &other) const -> Ivec4;
  auto operator*=(int32_t scalar) -> Ivec4 &;
  auto operator*=(const Ivec4 &other) -> Ivec4 &;

  auto operator/(int32_t scalar) const -> Ivec4;
  auto operator/(const Ivec4 &other) const -> Ivec4;
  auto operator/=(int32_t scalar) -> Ivec4 &;
  auto operator/=(const Ivec4 &other) -> Ivec4 &;

  auto operator==(const Ivec4 &other) const -> bool;
  auto operator!=(const Ivec4 &other) const -> bool;

  auto operator[](uint32_t index) const -> int32_t;
  auto operator[](uint32_t index) -> int32_t &;

  constexpr Ivec4(int32_t x_val, int32_t y_val, int32_t z_val, int32_t w_val) : x(x_val), y(y_val), z(z_val), w(w_val) {}
  constexpr explicit Ivec4(int32_t val) : x(val), y(val), z(val), w(val) {}
  constexpr Ivec4() = default;
  explicit Ivec4(const Ivec2 &vec2, int32_t z_val = 0, int32_t w_val = 0);
  explicit Ivec4(const Ivec3 &vec3, int32_t w_val = 0);

  explicit Ivec4(const Uvec2 &vec2, int32_t z_val = 0, int32_t w_val = 0);
  explicit Ivec4(const Uvec3 &vec3, int32_t w_val = 0);
  explicit Ivec4(const Uvec4 &vec4);

  [[nodiscard]] auto ToString() const -> std::string;
  [[nodiscard]] auto Hash() const -> uint64_t;
  [[nodiscard]] auto Span() -> std::span<int32_t> { return {&x, 4}; }
  [[nodiscard]] auto Span() const -> std::span<const int32_t> { return {&x, 4}; }
  [[nodiscard]] auto Ptr() -> int32_t * { return &x; }
  [[nodiscard]] auto Ptr() const -> const int32_t * { return &x; }
};

auto Min(const Vec2 &vec_a, const Vec2 &vec_b) -> Vec2;
auto Min(const Vec3 &vec_a, const Vec3 &vec_b) -> Vec3;
auto Min(const Vec4 &vec_a, const Vec4 &vec_b) -> Vec4;

auto Max(const Vec2 &vec_a, const Vec2 &vec_b) -> Vec2;
auto Max(const Vec3 &vec_a, const Vec3 &vec_b) -> Vec3;
auto Max(const Vec4 &vec_a, const Vec4 &vec_b) -> Vec4;

auto Min(const Uvec2 &vec_a, const Uvec2 &vec_b) -> Uvec2;
auto Min(const Uvec3 &vec_a, const Uvec3 &vec_b) -> Uvec3;
auto Min(const Uvec4 &vec_a, const Uvec4 &vec_b) -> Uvec4;

auto Max(const Uvec2 &vec_a, const Uvec2 &vec_b) -> Uvec2;
auto Max(const Uvec3 &vec_a, const Uvec3 &vec_b) -> Uvec3;
auto Max(const Uvec4 &vec_a, const Uvec4 &vec_b) -> Uvec4;

auto Min(const Ivec2 &vec_a, const Ivec2 &vec_b) -> Ivec2;
auto Min(const Ivec3 &vec_a, const Ivec3 &vec_b) -> Ivec3;
auto Min(const Ivec4 &vec_a, const Ivec4 &vec_b) -> Ivec4;

auto Max(const Ivec2 &vec_a, const Ivec2 &vec_b) -> Ivec2;
auto Max(const Ivec3 &vec_a, const Ivec3 &vec_b) -> Ivec3;
auto Max(const Ivec4 &vec_a, const Ivec4 &vec_b) -> Ivec4;

struct Plane : public Vec4 {
  Plane() = default;
  constexpr Plane(float x_val, float y_val, float z_val, float w_val) : Vec4(x_val, y_val, z_val, w_val) {}
  constexpr explicit Plane(const Vec4 &vec4) : Vec4(vec4) {}
  constexpr explicit Plane(const Vec3 &normal, float distance) : Vec4(normal.x, normal.y, normal.z, distance) {}
  constexpr explicit Plane(const Vec3 &origin, const Vec3 &normal) : Vec4(normal.x, normal.y, normal.z, -normal.Dot(origin)) {}

  [[nodiscard]] auto Normal() const -> Vec3;
  [[nodiscard]] auto Distance() const -> float;
  [[nodiscard]] auto Normalize() const -> Plane;
  [[nodiscard]] auto Point() const -> Vec3;
  [[nodiscard]] auto DistanceToPoint(const Vec3 &point) const -> float;
  [[nodiscard]] auto IntersectRay(const Vec3 &origin, const Vec3 &direction) const -> std::optional<Vec3>;
};

} // namespace Math