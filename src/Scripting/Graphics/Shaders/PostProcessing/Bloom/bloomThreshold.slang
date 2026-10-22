import utils;

uniform Sampler2D.Handle mainScene;

uniform float threshold;

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 texCoord: TEXCOORDS) {
  float3 color = Sanitise(mainScene.SampleLevelZero(texCoord).rgb);

  float luminance = Luminance(color);

  color *= clamp(log(max(1.0, luminance - threshold + 1.0)), 0.0, 10.0);

  return float4(color, 1.0);
}
