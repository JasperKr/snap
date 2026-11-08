#include "ray.hpp"

#include <algorithm>
#include <cmath>

namespace Math {

auto Ray::PointAt(float time) const -> Math::Vec3 { return Origin + Direction * time; }

auto Ray::IntersectPlane(const Math::Plane &plane) const -> std::optional<Math::Vec3> {
  constexpr static float Epsilon = 1e-6F;

  float denom = plane.Normal().Dot(Direction);
  [[likely]]
  if (std::abs(denom) > Epsilon) {
    float time = (plane.Distance() - plane.Normal().Dot(Origin)) / denom;

    if (time >= 0.0F) {
      return PointAt(time);
    }
  }

  return std::nullopt;
}

auto Ray::IntersectPlane2Way(const Math::Plane &plane) const -> std::optional<Math::Vec3> {
  constexpr static float Epsilon = 1e-6F;

  float denom = plane.Normal().Dot(Direction);
  [[likely]]
  if (std::abs(denom) > Epsilon) {
    float time = (plane.Distance() - plane.Normal().Dot(Origin)) / denom;
    return PointAt(time);
  }

  return std::nullopt;
}

auto Ray::IntersectAABB(const Math::Vec3 &min, const Math::Vec3 &max) const -> std::optional<Math::Vec3> {
  float tmin = (min.x - Origin.x) / Direction.x;
  float tmax = (max.x - Origin.x) / Direction.x;

  if (tmin > tmax) {
    std::swap(tmin, tmax);
  }

  float tymin = (min.y - Origin.y) / Direction.y;
  float tymax = (max.y - Origin.y) / Direction.y;

  if (tymin > tymax) {
    std::swap(tymin, tymax);
  }

  if ((tmin > tymax) || (tymin > tmax)) {
    return std::nullopt;
  }

  tmin = std::max(tymin, tmin);
  tmax = std::min(tymax, tmax);

  float tzmin = (min.z - Origin.z) / Direction.z;
  float tzmax = (max.z - Origin.z) / Direction.z;

  if (tzmin > tzmax) {
    std::swap(tzmin, tzmax);
  }

  if ((tmin > tzmax) || (tzmin > tmax)) {
    return std::nullopt;
  }

  tmin = std::max(tzmin, tmin);
  tmax = std::min(tzmax, tmax);

  if (tmin < 0.0F && tmax < 0.0F) {
    return std::nullopt; // Intersection is behind the ray origin
  }

  float intersectionTime = (tmin >= 0.0F) ? tmin : tmax;
  return PointAt(intersectionTime);
}

auto Ray::IntersectSphere(const Math::Vec3 &center, float radius) const -> std::optional<Math::Vec3> {
  // NOLINTBEGIN

  Math::Vec3 originToCenter = Origin - center;
  float a = Direction.Dot(Direction);
  float b = 2.0F * originToCenter.Dot(Direction);
  float c = originToCenter.Dot(originToCenter) - radius * radius;
  float discriminant = (b * b) - (4 * a * c);

  if (discriminant < 0) {
    return std::nullopt; // No intersection
  }

  float t1 = (-b - std::sqrtf(discriminant)) / (2.0F * a);
  float t2 = (-b + std::sqrtf(discriminant)) / (2.0F * a);

  if (t1 >= 0.0F) {
    return PointAt(t1);
  }
  if (t2 >= 0.0F) {
    return PointAt(t2);
  }

  // NOLINTEND

  return std::nullopt; // Intersection is behind the ray origin
}

auto Ray::IntersectTriangle(const Math::Vec3 &vertex0, const Math::Vec3 &vertex1, const Math::Vec3 &vertex2) const -> std::optional<Math::Vec3> {
  // NOLINTBEGIN

  constexpr static float Epsilon = 1e-6F;

  Math::Vec3 edge1 = vertex1 - vertex0;
  Math::Vec3 edge2 = vertex2 - vertex0;

  Math::Vec3 h = Direction.Cross(edge2);
  float a = edge1.Dot(h);

  if (std::abs(a) < Epsilon) {
    return std::nullopt; // Ray is parallel to the triangle
  }

  float f = 1.0F / a;
  Math::Vec3 s = Origin - vertex0;
  float u = f * s.Dot(h);

  if (u < 0.0F || u > 1.0F) {
    return std::nullopt; // Intersection is outside the triangle
  }

  Math::Vec3 q = s.Cross(edge1);
  float v = f * Direction.Dot(q);

  if (v < 0.0F || u + v > 1.0F) {
    return std::nullopt; // Intersection is outside the triangle
  }

  float t = f * edge2.Dot(q);

  if (t > Epsilon) {
    return PointAt(t); // Intersection point
  }

  return std::nullopt; // Intersection is behind the ray origin

  // NOLINTEND
}

auto Ray::ClosestPoint(const Math::Vec3 &point) const -> Math::Vec3 {
  Math::Vec3 originToPoint = point - Origin;
  float time = originToPoint.Dot(Direction);
  return PointAt(time);
}

auto Ray::DistanceToPoint(const Math::Vec3 &point) const -> float {
  Math::Vec3 closestPoint = ClosestPoint(point);
  return (closestPoint - point).Length();
}

auto Ray::DistanceSqrToPoint(const Math::Vec3 &point) const -> float {
  Math::Vec3 closestPoint = ClosestPoint(point);
  return (closestPoint - point).LengthSqr();
}

auto Ray::ClosestPoint(const Ray &other) const -> Math::Vec3 {
  constexpr static float Epsilon = 1e-6F;
  // NOLINTBEGIN

  Math::Vec3 originToOrigin = other.Origin - Origin;
  float a = Direction.Dot(Direction);
  float b = Direction.Dot(other.Direction);
  float c = other.Direction.Dot(other.Direction);
  float d = Direction.Dot(originToOrigin);
  float e = other.Direction.Dot(originToOrigin);

  float denom = (a * c) - (b * b);

  if (std::abs(denom) < Epsilon) {
    return Origin; // Rays are parallel, return the origin of this ray
  }

  float s = ((b * e) - (c * d)) / denom;
  return PointAt(s);

  // NOLINTEND
}

auto Ray::DistanceToRay(const Ray &other) const -> float {
  Math::Vec3 closestPoint = ClosestPoint(other);
  return (closestPoint - other.ClosestPoint(closestPoint)).Length();
}

auto Ray::DistanceSqrToRay(const Ray &other) const -> float {
  Math::Vec3 closestPoint = ClosestPoint(other);
  return (closestPoint - other.ClosestPoint(closestPoint)).LengthSqr();
}

auto Ray::ClosestPointOnSegment(const Math::Vec3 &point, const Math::Vec3 &segmentStart, const Math::Vec3 &segmentEnd) -> Math::Vec3 {
  Math::Vec3 segmentDirection = segmentEnd - segmentStart;
  float segmentLengthSqr = segmentDirection.LengthSqr();

  if (segmentLengthSqr <= 0.0F) {
    return segmentStart; // Segment is a point
  }

  float time = (point - segmentStart).Dot(segmentDirection) / segmentLengthSqr;
  time = std::clamp(time, 0.0F, 1.0F);

  return segmentStart + segmentDirection * time;
}

auto Ray::ToString() const -> std::string { return "Ray(Origin: " + Origin.ToString() + ", Direction: " + Direction.ToString() + ")"; }

} // namespace Math