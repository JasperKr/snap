uniform Sampler2D.Handle mainScene;

uniform uint2 viewportSize;
uniform float intensity;

const static float2 Coords[9] = { float2(-1.0f, 1.0f),  float2(0.0f, 1.0f),
                                  float2(1.0f, 1.0f),   float2(-1.0f, 0.0f),
                                  float2(0.0f, 0.0f),   float2(1.0f, 0.0f),
                                  float2(-1.0f, -1.0f), float2(0.0f, -1.0f),
                                  float2(1.0f, -1.0f) };

const static float Weights[9] = { 0.0625f, 0.125f,  0.0625f, 0.125f, 0.25f,
                                  0.125f,  0.0625f, 0.125f,  0.0625f };

float3 Sample(float2 coords) { return mainScene.SampleLevelZero(coords).rgb; }

float4 UpSample(float2 coords) {
  float3 color = float3(0.0);

  uint textureWidth = 0;
  uint textureHeight = 0;

  mainScene.GetDimensions(textureWidth, textureHeight);

  float2 pixelSize = 1.0 / float2(textureWidth, textureHeight);

  [unroll]
  for (int i = 0; i < 9; i++) {
    color += Sample(Coords[i] * pixelSize + coords).rgb * Weights[i];
  }

  color *= intensity;

  return float4(color, 1.0);
}

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 tex_coord: TEXCOORDS) { return UpSample(tex_coord); }
