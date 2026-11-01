module pointLight;

import light;
import pixel;
import physicallyBased;
import raytracing;

public struct PointLight : ILight {
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

  public float3 GetIntensityAt(const float3 WorldPosition, const Pixel pixel,
                               const float3 ViewDirection) {
    float3 LightVector = Position.xyz - WorldPosition;
    float Attenuation = AttenuateCusp(length(LightVector), Range, 4.0f);

    if (Attenuation <= 0.0f) {
      return float3(0.0f);
    }

    LightVector = normalize(LightVector);
    float NoL = saturate(dot(pixel.Normal, LightVector));

    float3 Diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);
    float3 brdf = SampleBRDF(ViewDirection, LightVector, pixel.NoV,
                             pixel.Normal, pixel.Roughness, pixel.Fresnel0,
                             Diffuse, NoL, pixel.LambdaV);

    return Color * brdf * (Intensity * (NoL * Attenuation));
  }

  public bool TraceShadow(float3 WorldPosition, const float3 Normal,
                          uint2 screenCoords, const uint frameRandomState) {
    float3 LightVector = Position.xyz - WorldPosition;
    float Attenuation = AttenuateCusp(length(LightVector), Range, 4.0f);

    if (Attenuation <= 0.0f) {
      return false;
    }

    LightVector = normalize(LightVector);
    float NoL = saturate(dot(Normal, LightVector));

    if (NoL <= 0.0f) {
      return false;
    }

    RayDesc ray;
    ray.Origin = WorldPosition + Normal * 1e-3f;
    ray.Direction = LightVector;
    ray.TMin = 0.0f;
    ray.TMax = distance(Position.xyz, WorldPosition);

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

public uniform uint32_t PointLightCount;
public uniform StructuredBuffer<PointLight>.Handle PointLights;
