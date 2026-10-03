import camera;
import utils;
import constants;

#define ERP

#if defined(CUBE_MAP)
SamplerCube SkyboxTexture;
#elif defined(ERP)
Sampler2D SkyboxTexture;
#endif
Sampler2D DepthTexture;

#include "fullscreen.slang"

float2 GetERPUvs(float3 rayDirection) {
  float2 uv;
  uv.x = atan2(-rayDirection.z, rayDirection.x) * INV_TWO_PI + 0.5;
  uv.y = asin(-rayDirection.y) * INV_PI + 0.5;
  return uv;
}

struct FragmentOutput {
  float4 color : SV_Target0;
  float2 motion : SV_Target1;
};

[shader("fragment")]
FragmentOutput fragmentMain(float2 tex_coord: TEXCOORDS) {
  float depth = DepthTexture.SampleLevelZero(tex_coord).r;
  if (depth != DepthFarPlane) {
    discard;
  }

  float3 rayDirection = camera.GetRayDirection(tex_coord);

  float3 previous = camera.ReprojectToPreviousUVs(tex_coord, depth);
  float2 motion = PackMotionVector(tex_coord - previous.xy);

#if defined(CUBE_MAP)
  return FragmentOutput(SkyboxTexture.Sample(rayDirection), motion);
#elif defined(ERP)
  float2 uv = GetERPUvs(rayDirection);

  float2 duvdx = ddx(uv);
  float2 duvdy = ddy(uv);

  // fix seam discontinuity
  if (abs(duvdx.x) > 0.5)
    duvdx.x = -sign(duvdx.x) * (1.0 - abs(duvdx.x));
  if (abs(duvdy.x) > 0.5)
    duvdy.x = -sign(duvdy.x) * (1.0 - abs(duvdy.x));

  return FragmentOutput(SkyboxTexture.SampleGrad(uv, duvdx, duvdy), motion);
#endif

  // return float4(saturate(rayDirection * 0.5 + 0.5), 1.0);
}
