module spotLight;

import light;
import pixel;

public struct SpotLight : ILight {
  public Light Base;
  public float Range;
  public float InnerConeAngle;
  public float OuterConeAngle;

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

uniform uint32_t SpotLightCount;
uniform StructuredBuffer<SpotLight>.Handle SpotLights;
