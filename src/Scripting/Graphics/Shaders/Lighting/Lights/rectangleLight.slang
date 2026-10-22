module rectangleLight;

import light;
import pixel;
import physicallyBased;
import constants;
import utils;

float RectangleSolidAngle(half3 V0, half3 V1, half3 V2, half3 V3) {
  mediump half3 n0 = normalize(cross(V0, V1));
  mediump half3 n1 = normalize(cross(V1, V2));
  mediump half3 n2 = normalize(cross(V2, V3));
  mediump half3 n3 = normalize(cross(V3, V0));

  mediump half g0 = acos(clamp(dot(-n0, n1), -1.0H, 1.0H));
  mediump half g1 = acos(clamp(dot(-n1, n2), -1.0H, 1.0H));
  mediump half g2 = acos(clamp(dot(-n2, n3), -1.0H, 1.0H));
  mediump half g3 = acos(clamp(dot(-n3, n0), -1.0H, 1.0H));

  return (g0 + g1 + g2 + g3) - (2.0 * PI);
}

float3 RayPlaneIntersect(float3 rayOrigin, float3 rayDirection,
                         float3 planePoint, float3 planeNormal) {
  float denom = dot(rayDirection, planeNormal);

  if (abs(denom) < 1e-6) {
    return rayOrigin + rayDirection * 1e6;
  }

  float t = dot(planePoint - rayOrigin, planeNormal) / denom;

  return rayOrigin + rayDirection * t;
}

public struct RectangleLight : ILight {
  public Light Base;
  public float2 Size;
  public float Range;

  public float3 GetForward() {
    const float3 Forward = float3(0.0f, 0.0f, 1.0f);

    return Rotate(Forward, Base.Rotation);
  }

  public half3 GetForwardh() { return half3(GetForward()); }

  public float3 GetRight() {
    const float3 Right = float3(1.0f, 0.0f, 0.0f);

    return Rotate(Right, Base.Rotation);
  }

  public half3 GetRighth() { return half3(GetRight()); }

  public float3 GetUp() {
    const float3 Up = float3(0.0f, 1.0f, 0.0f);

    return Rotate(Up, Base.Rotation);
  }

  public half3 GetUph() { return half3(GetUp()); }

  public static override float3 Sample(const float3 WorldPosition,
                                       const uint LightIndex, const Pixel pixel,
                                       const float3 ViewDirection,
                                       const float NoV, const float LambdaV,
                                       inout uint state, uint2 screenCoords,
                                       uint frameRandomState) {
    RectangleLight light = RectangleLights[LightIndex];

    half2 halfSize = half2(light.Size) * 0.5h;
    half3 forward = light.GetForwardh();
    half3 right = light.GetRighth();
    half3 up = light.GetUph();

    half3 pixelToCenter = half3(light.Base.Position.xyz - WorldPosition);

    half3 v0 = normalize(pixelToCenter + halfSize.x * right + halfSize.y * up);
    half3 v1 = normalize(pixelToCenter - halfSize.x * right + halfSize.y * up);
    half3 v2 = normalize(pixelToCenter - halfSize.x * right - halfSize.y * up);
    half3 v3 = normalize(pixelToCenter + halfSize.x * right - halfSize.y * up);

    float3 Radiance = light.Base.Color * light.Base.Intensity;
    float solidAngle = RectangleSolidAngle(v0, v1, v2, v3);

    pixelToCenter = normalize(pixelToCenter);

    float NoL =
        solidAngle * 0.2f *
        (saturate(dot(v0, pixel.Normal)) + saturate(dot(v1, pixel.Normal)) +
         saturate(dot(v2, pixel.Normal)) + saturate(dot(v3, pixel.Normal)) +
         saturate(dot(pixelToCenter, pixel.Normal)));

    return Radiance * NoL;
  }

  public static override float3 SampleAll(const float3 WorldPosition,
                                          const Pixel pixel,
                                          const float3 ViewDirection,
                                          const float NoV, const float LambdaV,
                                          inout uint state, uint2 screenCoords,
                                          uint frameRandomState) {
    return float3(0.0f);
  }

  public static override bool TraceShadow(const float3 WorldPosition,
                                          const uint LightIndex,
                                          const float3 Normal,
                                          uint2 screenCoords,
                                          const uint frameRandomState) {
    return false;
  }

  public Light GetLight() { return Base; }
}

uniform uint32_t RectangleLightCount;
uniform StructuredBuffer<RectangleLight>.Handle RectangleLights;
