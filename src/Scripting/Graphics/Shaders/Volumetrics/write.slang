#include "fullscreen.slang"

struct FragmentOutput {
  float4 color : SV_Target0;
};

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
  float4 screen_position : SV_Position;
};

uniform Sampler2D<float4>.Handle Volumetrics;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float4 incomingLightTransmittance = Volumetrics.Sample(input.tex_coord);
  return float4(incomingLightTransmittance.rgb, 1.0 - incomingLightTransmittance.a);
}
