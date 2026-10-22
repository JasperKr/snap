uniform Sampler2D.Handle mainScene;

// better bloom weights, credit to `Schmleeb` on discord
const static float Weights[13] = float[13](
    0.125, 0.125, 0.125, 0.125,

    0.03125, 0.0625, 0.03125, 0.0625, 0.125, 0.0625, 0.03125, 0.0625, 0.03125);

const static float2 Coords[13] = { float2(-1.0f, 1.0f),  float2(1.0f, 1.0f),
                                   float2(-1.0f, -1.0f), float2(1.0f, -1.0f),

                                   float2(-2.0f, 2.0f),  float2(0.0f, 2.0f),
                                   float2(2.0f, 2.0f),   float2(-2.0f, 0.0f),
                                   float2(0.0f, 0.0f),   float2(2.0f, 0.0f),
                                   float2(-2.0f, -2.0f), float2(0.0f, -2.0f),
                                   float2(2.0f, -2.0f) };

float3 Sample(int i, float2 coords) {
  return Weights[i] * mainScene.Sample(coords).rgb;
}

uniform uint2 viewportSize;

float4 DownSample(float2 coords, float2 texelSize) {
  float3 color = float3(0.0);

  [unroll]
  for (int i = 0; i < 13; i++) {
    color += Sample(i, coords + Coords[i] * texelSize);
  }

  return float4(color, 1.0);
}

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 tex_coord: TEXCOORDS) {
  float2 texelSize = 1.0 / float2(viewportSize);
  texelSize *= 0.5; // We render at half-res, so pixels are twice as big

  return DownSample(tex_coord, texelSize);
}
