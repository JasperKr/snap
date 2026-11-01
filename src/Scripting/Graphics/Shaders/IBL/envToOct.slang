uniform SamplerCube.Handle EnvironmentTexture;

#include "fullscreen.slang"

import constants;
import helpers;

uniform highp float Lod;

float signNotZero(float x) { return x < 0.0 ? -1.0 : 1.0; }

float2 signNotZero(float2 v) {
  return float2(signNotZero(v.x), signNotZero(v.y));
}

float3 DecodeOctahedron(float2 e) {
  e = e * 2.0 - 1.0;
  float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z <= 0.0)
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

struct FragmentInput {
  float2 VertexCoords : TEXCOORD0;
};

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float3 normal = DecodeOctahedron(input.VertexCoords);
  return EnvironmentTexture.SampleLevel(normal, Lod);
}
