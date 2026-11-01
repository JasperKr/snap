uniform float Lod;

uniform Sampler2D.Handle mainScene;

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(float2 tex_coord: TEXCOORDS) {
  float abberationScale = length(tex_coord - 0.5) + 0.5;
  float lod = Lod;
  float offset = 0.005 * abberationScale;

  float r = mainScene.SampleLevel(saturate(tex_coord - offset), lod).x;
  float g = mainScene.SampleLevel(saturate(tex_coord), lod).x;
  float b = mainScene.SampleLevel(saturate(tex_coord + offset), lod).x;

  return float4(r, g, b, 1.0) + 0.5;
}
