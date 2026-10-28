// Simple deferred lighting shader that just for-loops over all lights and accumulates their contribution.

import camera;
import physicallyBased;
import utils;
import pixel;
import constants;

import Lights.directionalLight;
import Lights.pointLight;
import Lights.spotLight;

#include "fullscreen.slang"

struct FragmentOutput {
  float4 color : SV_Target0;
};

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
  float4 screen_position : SV_Position;
};

uniform Sampler2D.Handle AlbedoTexture;
uniform Sampler2D.Handle NormalTexture;
uniform Sampler2D.Handle MaterialTexture;
uniform Sampler2D.Handle DepthTexture;
uniform Sampler2D.Handle EmissiveTexture;

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
  FragmentOutput out;

  float3 albedo = AlbedoTexture.Sample(input.tex_coord).rgb;
  float3 normalData = NormalTexture.Sample(input.tex_coord).xyz;
  MaterialChannel material =
      MaterialChannel(MaterialTexture.Sample(input.tex_coord));
  float3 emissive = EmissiveTexture.Sample(input.tex_coord).rgb;
  float depth = DepthTexture.Sample(input.tex_coord).r;

  if (depth == DepthFarPlane) {
    discard;
  }

  float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);

  float3 incomingLight = emissive.rgb;

  float3 viewDir = -CameraData[0].GetRayDirection(input.tex_coord);
  float3 worldPosition = CameraData[0].GetWorldPosition(input.tex_coord, depth);

  Pixel pixel;
  pixel.Normal = normal;
  pixel.Albedo = albedo.rgb;
  pixel.Metallic = material.metallic;
  pixel.PerceptualRoughness = material.perceptualRoughness;
  pixel.Roughness = material.roughness;
  pixel.Fresnel0 =
      ComputeFresnel0(albedo, material.metallic, material.reflectance);
  pixel.NoV = ClampNoV(dot(normal, viewDir));
  pixel.LambdaV = ComputeLambdaV(pixel.Roughness, pixel.NoV);

  uint2 screenCoords = uint2(input.screen_position.xy);

  for (int i = 0; i < DirectionalLightCount; i++) {
    var light = DirectionalLights.Load(i);

    incomingLight += light.GetIntensityAt(worldPosition, pixel, viewDir) *
                     light.SampleShadow(screenCoords);
  }

  // incomingLight +=
  //     PointLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV, state,
  //                          screenCoords, FrameRandomState);
  // incomingLight +=
  //     SpotLight.SampleAll(worldPosition, pixel, viewDir, NoV, LambdaV, state,
  //                         screenCoords, FrameRandomState);

  out.color = float4(incomingLight, 1.0f);

  return out;
}
