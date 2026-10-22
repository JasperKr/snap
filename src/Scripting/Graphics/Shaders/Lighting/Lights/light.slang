module light;

import pixel;

public struct Light {
  public float3 Color;
  public float Intensity;
  public int ShadowBufferIndex;

  public float4 Position;
  public float4 Rotation;
};

#ifndef SHADER_TYPE_SHADOW_SAMPLING
uniform Sampler2DArray<float>.Handle ShadowVisibilityTexture;
#endif

public interface ILight {
  Light GetLight();

  static float3 Sample(const float3 WorldPosition, const uint32_t LightIndex,
                       const Pixel pixel, const float3 ViewDirection,
                       const float NoV, const float LambdaV, inout uint state,
                       uint2 screenCoords, uint frameRandomState) {
    return float3(0.0f);
  }

  static float3 SampleAll(const float3 WorldPosition, const Pixel pixel,
                          const float3 ViewDirection, const float NoV,
                          const float LambdaV, inout uint state,
                          uint2 screenCoords, uint frameRandomState) {
    return float3(0.0f);
  }

  static bool TraceShadow(const float3 WorldPosition, const uint LightIndex,
                          const float3 Normal, uint2 screenCoords,
                          const uint frameRandomState) {
    return false;
  }

  static float ReadShadowVisibility(const int ShadowBufferIndex,
                                    const uint2 screenCoords) {
    if (ShadowBufferIndex < 0) {
      return 1.0f;
    }

    return ShadowVisibilityTexture.Load(
        int4(screenCoords, 0, ShadowBufferIndex));
  }
};

public float AttenuateCusp(float distance, float radius, float falloff) {
  float s = distance / radius;

  if (s >= 1.0) {
    return 0.0;
  }

  float s2 = s * s;
  float iS2 = 1.0 - s2;

  return (iS2 * iS2) / (1.0 + falloff * s);
}
