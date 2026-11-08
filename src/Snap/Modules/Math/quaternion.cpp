#include "quaternion.hpp"
#include <cmath>
#include <sstream>

namespace Math {
[[nodiscard]] auto Quaternion::Normalize() const -> Quaternion {
  float length = std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
  return {x / length, y / length, z / length, w / length};
}

[[nodiscard]] auto Quaternion::Conjugate() const -> Quaternion { return {-x, -y, -z, w}; }

[[nodiscard]] auto Quaternion::Inverse() const -> Quaternion {
  float lengthSq = (x * x) + (y * y) + (z * z) + (w * w);
  Quaternion conj = Conjugate();
  return {conj.x / lengthSq, conj.y / lengthSq, conj.z / lengthSq, conj.w / lengthSq};
}

[[nodiscard]] auto Quaternion::Dot(const Quaternion &other) const -> float { return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w); }

[[nodiscard]] auto Quaternion::Multiply(const Quaternion &other) const -> Quaternion {
  return {
      (w * other.x) + (x * other.w) + (y * other.z) - (z * other.y),
      (w * other.y) - (x * other.z) + (y * other.w) + (z * other.x),
      (w * other.z) - (x * other.y) + (y * other.x) + (z * other.w),
      (w * other.w) - (x * other.x) - (y * other.y) - (z * other.z),
  };
}

/*
public float3 Rotate(float3 v, float4 q) {
  return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
*/

[[nodiscard]] auto Quaternion::RotateVector(const Vec3 &vec) const -> Vec3 {
  Vec3 qVec{x, y, z};
  Vec3 quatCVec = qVec.Cross(vec);
  Vec3 vecCCVec = qVec.Cross(quatCVec);
  quatCVec *= (2.0F * w); // NOLINT
  vecCCVec *= 2.0F;       // NOLINT
  return vec + quatCVec + vecCCVec;
}

[[nodiscard]] auto Quaternion::ToString() const -> std::string {
  std::ostringstream oss;
  oss << "Quaternion(" << x << ", " << y << ", " << z << ", " << w << ")";
  return oss.str();
}

} // namespace Math