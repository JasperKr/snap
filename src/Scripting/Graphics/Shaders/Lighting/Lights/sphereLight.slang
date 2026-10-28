module sphereLight;

import light;
import pixel;

public struct SphereLight : ILight {
  public float3 Color;
  public float Intensity;
  public int ShadowBufferIndex;

  public float4 Position;
  public float4 Rotation;

  public float3 GetPosition() { return Position.xyz; }
  public float4 GetRotation() { return Rotation; }
  public float3 GetColor() { return Color; }
  public float GetIntensity() { return Intensity; }

  public float Range;
  public float Radius;

  public float3 GetIntensityAt(const float3 WorldPosition, const Pixel pixel,
                               const float3 ViewDirection) {
    return float3(0.0f);
  }

  public bool TraceShadow(const float3 WorldPosition, const float3 Normal,
                          uint2 screenCoords, const uint frameRandomState) {
    return false;
  }

  public float SampleShadow(int2 screenCoords) {
    if (ShadowBufferIndex < 0) {
      return 1.0f;
    }

    return ShadowVisibilityTexture.Load(
        int4(screenCoords, 0, ShadowBufferIndex));
  }
}

public uniform uint32_t SphereLightCount;
public uniform StructuredBuffer<SphereLight>.Handle SphereLights;
