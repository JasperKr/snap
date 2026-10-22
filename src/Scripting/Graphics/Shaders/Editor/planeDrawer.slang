import camera;
import constants;

uniform float4 plane;
uniform float4 color;

float RayPlaneIntersection(float3 rayOrigin, float3 rayDirection,
                           float4 plane) {
  float3 planeNormal = plane.xyz;
  float planeDistance = plane.w;

  float denominator = dot(planeNormal, rayDirection);
  if (abs(denominator) < 1e-6) {
    return -1.0; // No intersection, the ray is parallel to the plane
  }

  float t = -(dot(planeNormal, rayOrigin) + planeDistance) / denominator;
  return t >= 0.0
             ? t
             : -1.0; // Return -1 if the intersection is behind the ray origin
}

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 texcoords: TEXCOORDS) {
  float3 rayDirection = CameraData[0].GetRayDirection(texcoords);
  float3 rayOrigin = CameraData[0].Position;

  float t = RayPlaneIntersection(rayOrigin, rayDirection, plane);
  if (t < 0.0) {
    discard; // No intersection, discard the fragment
  }

  return color;
}
