#include "math.hpp"

#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "SDL3/SDL_timer.h"
#include "stb/stb_perlin.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <random>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
namespace Math {

namespace Conversions {
// See: https://www.euclideanspace.com/maths/geometry/rotations

auto ToEuler(Quaternion quaternion) -> EulerAngle {
  // YXZ rotation order: Yaw (Y-axis), Pitch (X-axis), Roll (Z-axis)
  float sqw = quaternion.w * quaternion.w;
  float sqx = quaternion.x * quaternion.x;
  float sqy = quaternion.y * quaternion.y;
  float sqz = quaternion.z * quaternion.z;

  // Gimbal lock test: sin(pitch) = 2*(w*x - y*z)
  float sinPitch = 2.0F * ((quaternion.w * quaternion.x) - (quaternion.y * quaternion.z));
  EulerAngle result;

  if (sinPitch > 0.999F) { // singularity at north pole (pitch = +90°)
    result.pitch = std::numbers::pi_v<float> / 2.0F;
    result.yaw = 2.0F * std::atan2(quaternion.y, quaternion.w);
    result.roll = 0;
    return result;
  }

  if (sinPitch < -0.999F) { // singularity at south pole (pitch = -90°)
    result.pitch = -std::numbers::pi_v<float> / 2.0F;
    result.yaw = 2.0F * std::atan2(quaternion.y, quaternion.w);
    result.roll = 0;
    return result;
  }

  // General case
  result.pitch = std::asin(sinPitch);
  result.yaw = std::atan2(2.0F * ((quaternion.x * quaternion.z) + (quaternion.w * quaternion.y)), 1.0F - (2.0F * (sqx + sqy)));
  result.roll = std::atan2(2.0F * ((quaternion.x * quaternion.y) + (quaternion.w * quaternion.z)), 1.0F - (2.0F * (sqx + sqz)));

  return result;
}

auto ToQuaternion(EulerAngle euler) -> Quaternion {
  // YXZ rotation order: Yaw (Y-axis), Pitch (X-axis), Roll (Z-axis)
  // q = q_yaw * q_pitch * q_roll
  float cos_yaw = std::cos(euler.yaw * 0.5F);
  float sin_yaw = std::sin(euler.yaw * 0.5F);
  float cos_pitch = std::cos(euler.pitch * 0.5F);
  float sin_pitch = std::sin(euler.pitch * 0.5F);
  float cos_roll = std::cos(euler.roll * 0.5F);
  float sin_roll = std::sin(euler.roll * 0.5F);

  Quaternion quat;
  quat.w = (cos_yaw * cos_pitch * cos_roll) + (sin_yaw * sin_pitch * sin_roll);
  quat.x = (cos_yaw * sin_pitch * cos_roll) + (sin_yaw * cos_pitch * sin_roll);
  quat.y = (sin_yaw * cos_pitch * cos_roll) - (cos_yaw * sin_pitch * sin_roll);
  quat.z = (cos_yaw * cos_pitch * sin_roll) - (sin_yaw * sin_pitch * cos_roll);

  return quat;
}

auto ToMatrix(EulerAngle euler) -> Matrix4x4 {
  // YXZ rotation order: R = Ry(yaw) * Rx(pitch) * Rz(roll)
  // Assuming the angles are in radians.
  float cos_yaw = std::cos(euler.yaw);
  float sin_yaw = std::sin(euler.yaw);
  float cos_pitch = std::cos(euler.pitch);
  float sin_pitch = std::sin(euler.pitch);
  float cos_roll = std::cos(euler.roll);
  float sin_roll = std::sin(euler.roll);

  Matrix4x4 mat{}; // Identity

  mat.At(0, 0) = (cos_yaw * cos_roll) + (sin_yaw * sin_pitch * sin_roll);
  mat.At(1, 0) = (-cos_yaw * sin_roll) + (sin_yaw * sin_pitch * cos_roll);
  mat.At(2, 0) = sin_yaw * cos_pitch;

  mat.At(0, 1) = cos_pitch * sin_roll;
  mat.At(1, 1) = cos_pitch * cos_roll;
  mat.At(2, 1) = -sin_pitch;

  mat.At(0, 2) = (-sin_yaw * cos_roll) + (cos_yaw * sin_pitch * sin_roll);
  mat.At(1, 2) = (sin_yaw * sin_roll) + (cos_yaw * sin_pitch * cos_roll);
  mat.At(2, 2) = cos_yaw * cos_pitch;

  return mat;
}

auto Tomatrix3x3(EulerAngle euler) -> Matrix3x3 {
  // YXZ rotation order: R = Ry(yaw) * Rx(pitch) * Rz(roll)
  // Assuming the angles are in radians.
  float cos_yaw = std::cos(euler.yaw);
  float sin_yaw = std::sin(euler.yaw);
  float cos_pitch = std::cos(euler.pitch);
  float sin_pitch = std::sin(euler.pitch);
  float cos_roll = std::cos(euler.roll);
  float sin_roll = std::sin(euler.roll);

  Matrix3x3 mat{}; // Identity

  mat.At(0, 0) = (cos_yaw * cos_roll) + (sin_yaw * sin_pitch * sin_roll);
  mat.At(1, 0) = (-cos_yaw * sin_roll) + (sin_yaw * sin_pitch * cos_roll);
  mat.At(2, 0) = sin_yaw * cos_pitch;

  mat.At(0, 1) = cos_pitch * sin_roll;
  mat.At(1, 1) = cos_pitch * cos_roll;
  mat.At(2, 1) = -sin_pitch;

  mat.At(0, 2) = (-sin_yaw * cos_roll) + (cos_yaw * sin_pitch * sin_roll);
  mat.At(1, 2) = (sin_yaw * sin_roll) + (cos_yaw * sin_pitch * cos_roll);
  mat.At(2, 2) = cos_yaw * cos_pitch;

  return mat;
}

auto ToEuler(Matrix4x4 matrix) -> EulerAngle {
  // YXZ rotation order extraction
  // mat[1][2] = -sin(pitch), mat[1][0] = cos(pitch)*sin(roll),
  // mat[1][1] = cos(pitch)*cos(roll), mat[0][2] = sin(yaw)*cos(pitch),
  // mat[2][2] = cos(yaw)*cos(pitch)
  EulerAngle result;

  float sinPitch = -matrix.At(2, 1);

  if (sinPitch > 0.999F) { // singularity at north pole (pitch = +90°)
    result.pitch = std::numbers::pi_v<float> / 2.0F;
    result.yaw = std::atan2(matrix.At(1, 0), matrix.At(0, 0));
    result.roll = 0;
    return result;
  }

  if (sinPitch < -0.999F) { // singularity at south pole (pitch = -90°)
    result.pitch = -std::numbers::pi_v<float> / 2.0F;
    result.yaw = std::atan2(-matrix.At(1, 0), matrix.At(0, 0));
    result.roll = 0;
    return result;
  }

  result.pitch = std::asin(sinPitch);
  result.yaw = std::atan2(matrix.At(2, 0), matrix.At(2, 2));
  result.roll = std::atan2(matrix.At(0, 1), matrix.At(1, 1));

  return result;
}

auto ToQuaternion(Matrix4x4 matrix) -> Quaternion {
  Quaternion quat;

  matrix = matrix;

  float trace = matrix.At(0, 0) + matrix.At(1, 1) + matrix.At(2, 2); // I removed + 1.0f; see discussion with Ethan
  if (trace > 0) {                                                   // I changed M_EPSILON to 0
    float scale = 0.5F / std::sqrt(trace + 1.0F);
    quat.w = 0.25F / scale;
    quat.x = (matrix.At(1, 2) - matrix.At(2, 1)) * scale;
    quat.y = (matrix.At(2, 0) - matrix.At(0, 2)) * scale;
    quat.z = (matrix.At(0, 1) - matrix.At(1, 0)) * scale;
  } else {
    if (matrix.At(0, 0) > matrix.At(1, 1) && matrix.At(0, 0) > matrix.At(2, 2)) {
      float scale = 2.0F * std::sqrt(1.0F + matrix.At(0, 0) - matrix.At(1, 1) - matrix.At(2, 2));
      quat.w = (matrix.At(1, 2) - matrix.At(2, 1)) / scale;
      quat.x = 0.25F * scale;
      quat.y = (matrix.At(1, 0) + matrix.At(0, 1)) / scale;
      quat.z = (matrix.At(2, 0) + matrix.At(0, 2)) / scale;
    } else if (matrix.At(1, 1) > matrix.At(2, 2)) {
      float scale = 2.0F * std::sqrt(1.0F + matrix.At(1, 1) - matrix.At(0, 0) - matrix.At(2, 2));
      quat.w = (matrix.At(2, 0) - matrix.At(0, 2)) / scale;
      quat.x = (matrix.At(1, 0) + matrix.At(0, 1)) / scale;
      quat.y = 0.25F * scale;
      quat.z = (matrix.At(2, 1) + matrix.At(1, 2)) / scale;
    } else {
      float scale = 2.0F * std::sqrt(1.0F + matrix.At(2, 2) - matrix.At(0, 0) - matrix.At(1, 1));
      quat.w = (matrix.At(0, 1) - matrix.At(1, 0)) / scale;
      quat.x = (matrix.At(2, 0) + matrix.At(0, 2)) / scale;
      quat.y = (matrix.At(2, 1) + matrix.At(1, 2)) / scale;
      quat.z = 0.25F * scale;
    }
  }

  return quat;
}

auto ToMatrix(Quaternion quat) -> Matrix4x4 {
  float sqw = quat.w * quat.w;
  float sqx = quat.x * quat.x;
  float sqy = quat.y * quat.y;
  float sqz = quat.z * quat.z;

  Matrix4x4 mat{}; // Identity

  // invs (inverse square length) is only required if quaternion is not already normalised
  float invs = 1.0F / (sqx + sqy + sqz + sqw);

  // since sqw + sqx + sqy + sqz =1/invs*invs
  mat.At(0, 0) = (sqx - sqy - sqz + sqw) * invs;
  mat.At(1, 1) = (-sqx + sqy - sqz + sqw) * invs;
  mat.At(2, 2) = (-sqx - sqy + sqz + sqw) * invs;

  float tmp1 = quat.x * quat.y;
  float tmp2 = quat.z * quat.w;
  mat.At(0, 1) = 2.0F * (tmp1 + tmp2) * invs;
  mat.At(1, 0) = 2.0F * (tmp1 - tmp2) * invs;

  tmp1 = quat.x * quat.z;
  tmp2 = quat.y * quat.w;
  mat.At(0, 2) = 2.0F * (tmp1 - tmp2) * invs;
  mat.At(2, 0) = 2.0F * (tmp1 + tmp2) * invs;
  tmp1 = quat.y * quat.z;
  tmp2 = quat.x * quat.w;
  mat.At(1, 2) = 2.0F * (tmp1 + tmp2) * invs;
  mat.At(2, 1) = 2.0F * (tmp1 - tmp2) * invs;

  return mat;
}

auto ToMatrix3x3(Quaternion quat) -> Matrix3x3 {
  float sqw = quat.w * quat.w;
  float sqx = quat.x * quat.x;
  float sqy = quat.y * quat.y;
  float sqz = quat.z * quat.z;

  Matrix3x3 mat{}; // Identity

  // invs (inverse square length) is only required if quaternion is not already normalised
  float invs = 1.0F / (sqx + sqy + sqz + sqw);

  // since sqw + sqx + sqy + sqz =1/invs*invs
  mat.At(0, 0) = (sqx - sqy - sqz + sqw) * invs;
  mat.At(1, 1) = (-sqx + sqy - sqz + sqw) * invs;
  mat.At(2, 2) = (-sqx - sqy + sqz + sqw) * invs;

  float tmp1 = quat.x * quat.y;
  float tmp2 = quat.z * quat.w;
  mat.At(0, 1) = 2.0F * (tmp1 + tmp2) * invs;
  mat.At(1, 0) = 2.0F * (tmp1 - tmp2) * invs;

  tmp1 = quat.x * quat.z;
  tmp2 = quat.y * quat.w;
  mat.At(0, 2) = 2.0F * (tmp1 - tmp2) * invs;
  mat.At(2, 0) = 2.0F * (tmp1 + tmp2) * invs;
  tmp1 = quat.y * quat.z;
  tmp2 = quat.x * quat.w;
  mat.At(1, 2) = 2.0F * (tmp1 + tmp2) * invs;
  mat.At(2, 1) = 2.0F * (tmp1 - tmp2) * invs;

  return mat;
}

} // namespace Conversions

// NOLINTNEXTLINE
thread_local std::mt19937 rng{std::random_device{}() ^ SDL_GetTicksNS()}; // Just to be safe, we XOR the ticks as well.

auto Random(int Min, int Max) -> int {
  Min = std::min(Min, Max);

  std::uniform_int_distribution<int> dist(Min, Max);
  return dist(rng);
}

auto Random(float Min, float Max) -> float {
  Min = std::min(Min, Max);

  std::uniform_real_distribution<float> dist(Min, Max);
  return dist(rng);
}

auto Random(long Min, long Max) -> long {
  Min = std::min(Min, Max);

  std::uniform_int_distribution<long> dist(Min, Max);
  return dist(rng);
}

auto Random(int Max) -> int { return Random(0, Max); }
auto Random() -> float {
  std::uniform_real_distribution<float> dist(0.0F, 1.0F);
  return dist(rng);
}
auto RandomNormalDistribution(float mean, float stddev) -> float {
  std::normal_distribution<float> dist(mean, stddev);
  return dist(rng);
}

auto Noise(float x_channel, uint x_wrap) -> float {
  // remove all but the highest bit of wrap and limit to 256
  x_wrap = (x_wrap & -x_wrap) % 256U;
  return stb_perlin_noise3(x_channel, 0.0F, 0.0F, static_cast<int>(x_wrap), 0, 0);
}
auto Noise(float x_channel, float y_channel, uint x_wrap, uint y_wrap) -> float {
  x_wrap = (x_wrap & -x_wrap) % 256U;
  y_wrap = (y_wrap & -y_wrap) % 256U;
  return stb_perlin_noise3(x_channel, y_channel, 0.0F, static_cast<int>(x_wrap), static_cast<int>(y_wrap), 0);
}
auto Noise(float x_channel, float y_channel, float z_channel, uint x_wrap, uint y_wrap, uint z_wrap) -> float {
  x_wrap = (x_wrap & -x_wrap) % 256U;
  y_wrap = (y_wrap & -y_wrap) % 256U;
  z_wrap = (z_wrap & -z_wrap) % 256U;
  return stb_perlin_noise3(x_channel, y_channel, z_channel, static_cast<int>(x_wrap), static_cast<int>(y_wrap), static_cast<int>(z_wrap));
}

/*
auto Abs(float value) -> float;
auto Abs(Vec2 vec) -> Vec2;
auto Abs(Vec3 vec) -> Vec3;
auto Abs(Vec4 vec) -> Vec4;
auto Abs(Uvec2 vec) -> Uvec2;
auto Abs(Uvec3 vec) -> Uvec3;
auto Abs(Uvec4 vec) -> Uvec4;
auto Abs(Ivec2 vec) -> Ivec2;
auto Abs(Ivec3 vec) -> Ivec3;
auto Abs(Ivec4 vec) -> Ivec4;
auto Abs(Matrix3x3 mat) -> Matrix3x3;
auto Abs(Matrix4x4 mat) -> Matrix4x4;

auto Floor(float value) -> float;
auto Floor(Vec2 vec) -> Vec2;
auto Floor(Vec3 vec) -> Vec3;
auto Floor(Vec4 vec) -> Vec4;
auto Floor(Uvec2 vec) -> Uvec2;
auto Floor(Uvec3 vec) -> Uvec3;
auto Floor(Uvec4 vec) -> Uvec4;
auto Floor(Ivec2 vec) -> Ivec2;
auto Floor(Ivec3 vec) -> Ivec3;
auto Floor(Ivec4 vec) -> Ivec4;

auto Ceil(float value) -> float;
auto Ceil(Vec2 vec) -> Vec2;
auto Ceil(Vec3 vec) -> Vec3;
auto Ceil(Vec4 vec) -> Vec4;
auto Ceil(Uvec2 vec) -> Uvec2;
auto Ceil(Uvec3 vec) -> Uvec3;
auto Ceil(Uvec4 vec) -> Uvec4;
auto Ceil(Ivec2 vec) -> Ivec2;
auto Ceil(Ivec3 vec) -> Ivec3;
auto Ceil(Ivec4 vec) -> Ivec4;
*/

auto Abs(float value) -> float { return std::abs(value); }
auto Abs(Vec2 vec) -> Vec2 { return {std::abs(vec.x), std::abs(vec.y)}; }
auto Abs(Vec3 vec) -> Vec3 { return {std::abs(vec.x), std::abs(vec.y), std::abs(vec.z)}; }
auto Abs(Vec4 vec) -> Vec4 { return {std::abs(vec.x), std::abs(vec.y), std::abs(vec.z), std::abs(vec.w)}; }

auto Abs(Ivec2 vec) -> Ivec2 { return {std::abs(vec.x), std::abs(vec.y)}; }
auto Abs(Ivec3 vec) -> Ivec3 { return {std::abs(vec.x), std::abs(vec.y), std::abs(vec.z)}; }
auto Abs(Ivec4 vec) -> Ivec4 { return {std::abs(vec.x), std::abs(vec.y), std::abs(vec.z), std::abs(vec.w)}; }

auto Abs(Matrix3x3 mat) -> Matrix3x3 {
  Matrix3x3 result;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      result.At(i, j) = std::abs(mat.At(i, j));
    }
  }
  return result;
}

auto Abs(Matrix4x4 mat) -> Matrix4x4 {
  Matrix4x4 result;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      result.At(i, j) = std::abs(mat.At(i, j));
    }
  }
  return result;
}

auto Floor(float value) -> float { return std::floor(value); }
auto Floor(Vec2 vec) -> Vec2 { return {std::floor(vec.x), std::floor(vec.y)}; }
auto Floor(Vec3 vec) -> Vec3 { return {std::floor(vec.x), std::floor(vec.y), std::floor(vec.z)}; }
auto Floor(Vec4 vec) -> Vec4 { return {std::floor(vec.x), std::floor(vec.y), std::floor(vec.z), std::floor(vec.w)}; }

auto Floor(Uvec2 vec) -> Uvec2 { return {static_cast<uint>(std::floor(vec.x)), static_cast<uint>(std::floor(vec.y))}; }
auto Floor(Uvec3 vec) -> Uvec3 { return {static_cast<uint>(std::floor(vec.x)), static_cast<uint>(std::floor(vec.y)), static_cast<uint>(std::floor(vec.z))}; }
auto Floor(Uvec4 vec) -> Uvec4 {
  return {static_cast<uint>(std::floor(vec.x)), static_cast<uint>(std::floor(vec.y)), static_cast<uint>(std::floor(vec.z)),
          static_cast<uint>(std::floor(vec.w))};
}

auto Floor(Ivec2 vec) -> Ivec2 { return {static_cast<int>(std::floor(vec.x)), static_cast<int>(std::floor(vec.y))}; }
auto Floor(Ivec3 vec) -> Ivec3 { return {static_cast<int>(std::floor(vec.x)), static_cast<int>(std::floor(vec.y)), static_cast<int>(std::floor(vec.z))}; }
auto Floor(Ivec4 vec) -> Ivec4 {
  return {static_cast<int>(std::floor(vec.x)), static_cast<int>(std::floor(vec.y)), static_cast<int>(std::floor(vec.z)), static_cast<int>(std::floor(vec.w))};
}

auto Ceil(float value) -> float { return std::ceil(value); }
auto Ceil(Vec2 vec) -> Vec2 { return {std::ceil(vec.x), std::ceil(vec.y)}; }
auto Ceil(Vec3 vec) -> Vec3 { return {std::ceil(vec.x), std::ceil(vec.y), std::ceil(vec.z)}; }
auto Ceil(Vec4 vec) -> Vec4 { return {std::ceil(vec.x), std::ceil(vec.y), std::ceil(vec.z), std::ceil(vec.w)}; }

auto Ceil(Uvec2 vec) -> Uvec2 { return {static_cast<uint>(std::ceil(vec.x)), static_cast<uint>(std::ceil(vec.y))}; }
auto Ceil(Uvec3 vec) -> Uvec3 { return {static_cast<uint>(std::ceil(vec.x)), static_cast<uint>(std::ceil(vec.y)), static_cast<uint>(std::ceil(vec.z))}; }
auto Ceil(Uvec4 vec) -> Uvec4 {
  return {static_cast<uint>(std::ceil(vec.x)), static_cast<uint>(std::ceil(vec.y)), static_cast<uint>(std::ceil(vec.z)), static_cast<uint>(std::ceil(vec.w))};
}

auto Ceil(Ivec2 vec) -> Ivec2 { return {static_cast<int>(std::ceil(vec.x)), static_cast<int>(std::ceil(vec.y))}; }
auto Ceil(Ivec3 vec) -> Ivec3 { return {static_cast<int>(std::ceil(vec.x)), static_cast<int>(std::ceil(vec.y)), static_cast<int>(std::ceil(vec.z))}; }
auto Ceil(Ivec4 vec) -> Ivec4 {
  return {static_cast<int>(std::ceil(vec.x)), static_cast<int>(std::ceil(vec.y)), static_cast<int>(std::ceil(vec.z)), static_cast<int>(std::ceil(vec.w))};
}

//
} // namespace Math

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
