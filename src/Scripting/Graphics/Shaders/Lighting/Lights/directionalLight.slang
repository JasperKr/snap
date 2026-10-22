module directionalLight;

import light;
import pixel;
import utils;
import physicallyBased;
import raytracing;
import random;

public struct DirectionalLight : ILight {
  public Light Base;

  public Light GetLight() { return Base; }

  public static override float3 Sample(const float3 WorldPosition,
                                       const uint LightIndex, const Pixel pixel,
                                       const float3 ViewDirection,
                                       const float NoV, const float LambdaV,
                                       inout uint state, uint2 screenCoords,
                                       uint frameRandomState) {
    DirectionalLight light = DirectionalLights[LightIndex];
    float3 Color = light.Base.Color * light.Base.Intensity;

    const float3 LocalForward = float3(0.0f, 0.0f, 1.0f);
    const float3 LocalUp = float3(0.0f, 1.0f, 0.0f);
    const float3 WorldForward = Rotate(LocalForward, light.Base.Rotation);
    const float3 WorldUp = Rotate(LocalUp, light.Base.Rotation);

    float NdotL = saturate(dot(pixel.Normal, WorldForward));
    float3 Diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

    float3 brdf =
        SampleBRDF(ViewDirection, WorldForward, NoV, pixel.Normal,
                   pixel.Roughness, pixel.Fresnel0, Diffuse, NdotL, LambdaV);

#ifndef SHADER_TYPE_SHADOW_SAMPLING
    float visibility =
        ReadShadowVisibility(light.GetLight().ShadowBufferIndex, screenCoords);
#else
    float visibility = 1.0;
#endif

    return Color * brdf * visibility;
  }

  public static override bool TraceShadow(const float3 WorldPosition,
                                          const uint LightIndex,
                                          const float3 Normal,
                                          uint2 screenCoords,
                                          const uint frameRandomState) {
    DirectionalLight light = DirectionalLights[LightIndex];

    const float3 WorldForward =
        Rotate(float3(0.0f, 0.0f, 1.0f), light.Base.Rotation);
    const float3 WorldUp =
        Rotate(float3(0.0f, 1.0f, 0.0f), light.Base.Rotation);

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

  public static override float3 SampleAll(const float3 WorldPosition,
                                          const Pixel pixel,
                                          const float3 ViewDirection,
                                          const float NoV, const float LambdaV,
                                          inout uint state, uint2 screenCoords,
                                          uint frameRandomState) {
    float3 result = float3(0.0f);
    for (uint i = 0; i < DirectionalLightCount; i++) {
      result += Sample(WorldPosition, i, pixel, ViewDirection, NoV, LambdaV,
                       state, screenCoords, frameRandomState);
    }
    return result;
  }
}

uniform uint32_t DirectionalLightCount;
uniform StructuredBuffer<DirectionalLight>.Handle DirectionalLights;
