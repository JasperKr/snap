module sphereLight;

import light;
import pixel;

public struct SphereLight : ILight {
  public Light Base;
  public float Range;
  public float Radius;

  public Light GetLight() { return Base; }

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
}

uniform uint32_t SphereLightCount;
uniform StructuredBuffer<SphereLight>.Handle SphereLights;
