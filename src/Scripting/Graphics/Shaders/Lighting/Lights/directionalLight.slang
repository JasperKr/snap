module directionalLight;

import light;
import pixel;
import utils;
import physicallyBased;
import raytracing;
import random;

public struct DirectionalLight : ILight {
  public float3 Color;
  public float Intensity;
  public int ShadowBufferIndex;

  public float4 Position;
  public float4 Rotation;

  public float3 GetPosition() { return Position.xyz; }
  public float4 GetRotation() { return Rotation; }
  public float3 GetColor() { return Color; }
  public float GetIntensity() { return Intensity; }

  public float3 GetIntensityAt(const float3 WorldPosition, const Pixel pixel,
                               const float3 ViewDirection) {
    float3 Color = Color * Intensity;

    const float3 LocalForward = float3(0.0f, 0.0f, 1.0f);
    const float3 LocalUp = float3(0.0f, 1.0f, 0.0f);
    const float3 WorldForward = Rotate(LocalForward, Rotation);
    const float3 WorldUp = Rotate(LocalUp, Rotation);

    float NdotL = saturate(dot(pixel.Normal, WorldForward));
    float3 Diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

    float3 brdf = SampleBRDF(ViewDirection, WorldForward, pixel.NoV,
                             pixel.Normal, pixel.Roughness, pixel.Fresnel0,
                             Diffuse, NdotL, pixel.LambdaV);

    return Color * brdf;
  }

  public float SampleShadow(int2 screenCoords) {
    if (ShadowBufferIndex < 0) {
      return 1.0f;
    }

    return ShadowVisibilityTexture.Load(
        int4(screenCoords, 0, ShadowBufferIndex));
  }

  public bool TraceShadow(const float3 WorldPosition, const float3 Normal,
                          uint2 screenCoords, const uint frameRandomState) {
    const float3 WorldForward = Rotate(float3(0.0f, 0.0f, 1.0f), Rotation);
    const float3 WorldUp = Rotate(float3(0.0f, 1.0f, 0.0f), Rotation);

    float NoL = dot(Normal, WorldForward);
    if (NoL <= 0.0f) {
      return true;
    }

    screenCoords += uint2(frameRandomState, frameRandomState * 84313U);

    float rnd0 = BlueNoiseTexture.Load(int3(screenCoords % 128, 0)).r;
    screenCoords += uint2(frameRandomState * 3913U, frameRandomState);
    float rnd1 = BlueNoiseTexture.Load(int3(screenCoords % 128, 0)).r;

    float2 randomDirection = RandomPointOnUnitDisc2D(rnd0, rnd1);
    const float TanSunAngularRadius = 0.0080724;
    randomDirection *= TanSunAngularRadius;
    float3 right = normalize(cross(WorldForward, WorldUp));

    float3 sampleDirection = normalize(
        WorldForward + randomDirection.x * right + randomDirection.y * WorldUp);

    RayDesc ray;
    ray.Origin = WorldPosition + Normal * 1e-2f;
    ray.Direction = sampleDirection;
    ray.TMin = 0.01f;
    ray.TMax = 1e4f;

    return TraceAnyHitRay(ray);
  }
}

public uniform uint32_t DirectionalLightCount;
public uniform StructuredBuffer<DirectionalLight>.Handle DirectionalLights;
