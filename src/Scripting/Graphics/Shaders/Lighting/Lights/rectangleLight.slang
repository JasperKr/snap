module rectangleLight;

import light;
import pixel;
import physicallyBased;
import constants;
import utils;
import random;
import raytracing;

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
  public float3 Color;
  public float Intensity;
  public int ShadowBufferIndex;

  public float4 Position;
  public float4 Rotation;

  public float3 GetPosition() { return Position.xyz; }
  public float4 GetRotation() { return Rotation; }
  public float3 GetColor() { return Color; }
  public float GetIntensity() { return Intensity; }

  public float2 Size;
  public float Range;

  public float3 GetForward() {
    const float3 Forward = float3(0.0f, 0.0f, 1.0f);

    return Rotate(Forward, Rotation);
  }

  public half3 GetForwardh() { return half3(GetForward()); }

  public float3 GetRight() {
    const float3 Right = float3(1.0f, 0.0f, 0.0f);

    return Rotate(Right, Rotation);
  }

  public half3 GetRighth() { return half3(GetRight()); }

  public float3 GetUp() {
    const float3 Up = float3(0.0f, 1.0f, 0.0f);

    return Rotate(Up, Rotation);
  }

  public half3 GetUph() { return half3(GetUp()); }

  public float3 GetIntensityAt(const float3 WorldPosition, const Pixel pixel,
                               const float3 ViewDirection) {
    half2 halfSize = half2(Size) * 0.5h;
    half3 forward = GetForwardh();
    half3 right = GetRighth();
    half3 up = GetUph();

    half3 pixelToCenter = half3(Position.xyz - WorldPosition);

    half3 v0 = normalize(pixelToCenter + halfSize.x * right + halfSize.y * up);
    half3 v1 = normalize(pixelToCenter - halfSize.x * right + halfSize.y * up);
    half3 v2 = normalize(pixelToCenter - halfSize.x * right - halfSize.y * up);
    half3 v3 = normalize(pixelToCenter + halfSize.x * right - halfSize.y * up);

    float3 Radiance = Color * Intensity;
    float solidAngle = RectangleSolidAngle(v0, v1, v2, v3);

    pixelToCenter = normalize(pixelToCenter);

    float NoL =
        solidAngle * 0.2f *
        (saturate(dot(v0, pixel.Normal)) + saturate(dot(v1, pixel.Normal)) +
         saturate(dot(v2, pixel.Normal)) + saturate(dot(v3, pixel.Normal)) +
         saturate(dot(pixelToCenter, pixel.Normal)));

    return Radiance * NoL;
  }

  public bool TraceShadow(const float3 WorldPosition, const float3 Normal,
                          uint2 screenCoords, const uint frameRandomState) {
    screenCoords += uint2(frameRandomState, frameRandomState * 84313U);

    float rnd0 = BlueNoiseTexture.Load(int3(screenCoords % 128, 0)).r;
    screenCoords += uint2(frameRandomState * 3913U, frameRandomState);
    float rnd1 = BlueNoiseTexture.Load(int3(screenCoords % 128, 0)).r;

    float3 position =
        GetRight() * (rnd0 - 0.5f) * Size.x + GetUp() * (rnd1 - 0.5f) * Size.y;

    float3 sampleDirection = normalize(position - WorldPosition);

    RayDesc ray;
    ray.Origin = WorldPosition + Normal * 1e-2f;
    ray.Direction = sampleDirection;
    ray.TMin = 0.01f;
    ray.TMax = distance(position, WorldPosition);

    return TraceAnyHitRay(ray);
  }

  public float SampleShadow(int2 screenCoords) {
    if (ShadowBufferIndex < 0) {
      return 1.0f;
    }

    return ShadowVisibilityTexture.Load(
        int4(screenCoords, 0, ShadowBufferIndex));
  }
}

public uniform uint32_t RectangleLightCount;
public uniform StructuredBuffer<RectangleLight>.Handle RectangleLights;
