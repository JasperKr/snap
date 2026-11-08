module light;

import pixel;

public struct Light {};

public uniform Sampler2DArray<float>.Handle ShadowVisibilityTexture;

public interface ILight {
  float3 GetPosition();
  float4 GetRotation();
  float GetIntensity();
  float3 GetColor();

  float3 GetIntensityAt(float3 WorldPosition, Pixel pixel, float3 ViewDirection);

  bool TraceShadow(float3 WorldPosition, float3 Normal, uint2 screenCoords, uint frameRandomState);

  float SampleShadow(int2 screenCoords);
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
