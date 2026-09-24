#pragma once

#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
namespace Math {

struct Ray {
  Math::Vec3 Origin;
  Math::Vec3 Direction;

  explicit Ray(const Math::Vec3 &origin, const Math::Vec3 &direction)
      : Origin(origin), Direction(direction.Normalize()) {}
  explicit Ray(const Math::Matrix4x4 &inverseViewProjectionMatrix,
               const Math::Vec2 &ndc) {
    Origin = Math::Vec3(inverseViewProjectionMatrix *
                        Math::Vec4(ndc.x, ndc.y, -1.0F, 1.0F));
    Math::Vec4 farPoint =
        inverseViewProjectionMatrix * Math::Vec4(ndc.x, ndc.y, 1.0F, 1.0F);
    Direction = Math::Vec3((farPoint - Math::Vec4(Origin, 1.0F)).Normalize());
  }
  Ray() = default;

  [[nodiscard]] auto PointAt(float time) const -> Math::Vec3;
  [[nodiscard]] auto IntersectPlane(const Math::Plane &plane) const
      -> std::optional<Math::Vec3>;
  [[nodiscard]] auto IntersectPlane2Way(const Math::Plane &plane) const
      -> std::optional<Math::Vec3>;

  [[nodiscard]] auto IntersectAABB(const Math::Vec3 &min,
                                   const Math::Vec3 &max) const
      -> std::optional<Math::Vec3>;
  [[nodiscard]] auto IntersectSphere(const Math::Vec3 &center,
                                     float radius) const
      -> std::optional<Math::Vec3>;
  [[nodiscard]] auto IntersectTriangle(const Math::Vec3 &vertex0,
                                       const Math::Vec3 &vertex1,
                                       const Math::Vec3 &vertex2) const
      -> std::optional<Math::Vec3>;
  [[nodiscard]] auto ClosestPoint(const Math::Vec3 &point) const -> Math::Vec3;
  [[nodiscard]] auto DistanceToPoint(const Math::Vec3 &point) const -> float;
  [[nodiscard]] auto DistanceSqrToPoint(const Math::Vec3 &point) const -> float;
  [[nodiscard]] auto ClosestPoint(const Ray &other) const -> Math::Vec3;
  [[nodiscard]] auto DistanceToRay(const Ray &other) const -> float;
  [[nodiscard]] auto DistanceSqrToRay(const Ray &other) const -> float;
  static auto ClosestPointOnSegment(const Math::Vec3 &point,
                                    const Math::Vec3 &segmentStart,
                                    const Math::Vec3 &segmentEnd) -> Math::Vec3;

  [[nodiscard]] auto ToString() const -> std::string;
};

} // namespace Math