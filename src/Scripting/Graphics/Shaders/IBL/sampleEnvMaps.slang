import constants;
import utils;
import camera;
import helpers;
import physicallyBased;

uniform Sampler2D.Handle DepthTexture;
uniform Sampler2D.Handle NormalTexture;
uniform Sampler2D.Handle AlbedoTexture;
uniform Sampler2D.Handle MaterialTexture;

uniform bool SpecularEnabled;

#include "fullscreen.slang"

struct FragmentOutput {
  float4 Radiance : SV_Target0;
  float4 Irradiance : SV_Target1;
};

struct LightProbe {
  float3 position;
  int index;

  float radius;
  float innerRadius;
};

float sdSphere(float3 center, float radius, float3 point) {
  return distance(center, point) - radius;
}

struct LightProbeSampleInfo {
  float3 radiance;
  float3 irradiance;
  float influence;
};

// TODO: Volume texture for the world, storing an index and count into
// a list of indices for probes that influence that cell, and then sample the probes in the shader
uniform int LightProbeCount;
uniform StructuredBuffer<LightProbe>.Handle Probes;

uniform Sampler2DArray<float3>.Handle SpecularTextures;
uniform Sampler2DArray<float3>.Handle IrradianceTextures;
uniform Sampler2D<float2>.Handle AmbientOcclusionTexture;

LightProbeSampleInfo SampleLightProbe(int probeIndex, float3 worldPosition,
                                      float3 normal, float3 reflectedRay,
                                      float perceptualRoughness) {
  float influence = 1.0;
  LightProbe probe = Probes[probeIndex];

  float signedDistMax = sdSphere(probe.position, probe.radius, worldPosition);
  if (signedDistMax > 0.0)
    return LightProbeSampleInfo(float3(0.0), float3(0.0), 0.0);

  float signedDistMin =
      sdSphere(probe.position, probe.innerRadius, worldPosition);
  if (signedDistMin > 0.0) {
    influence =
        1.0 - saturate(signedDistMin / (probe.radius - probe.innerRadius));
  }

  if (influence <= 0.0)
    return LightProbeSampleInfo(float3(0.0), float3(0.0), 0.0);

  float3 reflected = prefilteredRadiance(
      SpecularTextures, float4(reflectedRay, probe.index), perceptualRoughness);
  float3 irradiance =
      prefilteredIrradiance(IrradianceTextures, float4(normal, probe.index));

  return LightProbeSampleInfo(reflected, irradiance, influence);
}

LightProbeSampleInfo SampleLightProbes(float3 worldPosition, float3 normal,
                                       float3 reflectedRay,
                                       float perceptualRoughness) {
  LightProbeSampleInfo totalInfo;
  totalInfo.radiance = float3(0.0);
  totalInfo.irradiance = float3(0.0);
  totalInfo.influence = 0.0;

  for (int i = 0; i < LightProbeCount; i++) {
    LightProbeSampleInfo info = SampleLightProbe(
        i, worldPosition, normal, reflectedRay, perceptualRoughness);

    totalInfo.radiance += info.radiance * info.influence;
    totalInfo.irradiance += info.irradiance * info.influence;
    totalInfo.influence += info.influence;
  }

  return totalInfo;
}

[shader("fragment")]
FragmentOutput fragmentMain(float2 VertexCoords: TEXCOORD0) {
  float depth = DepthTexture.Sample(VertexCoords).r;

  if (depth == DepthFarPlane) {
    // TODO: Or discard?
    FragmentOutput output;
    output.Radiance = float4(0.0, 0.0, 0.0, 1.0);
    output.Irradiance = float4(0.0, 0.0, 0.0, 1.0);
    return output;
  }

  float3 worldPosition = CameraData[0].GetWorldPosition(VertexCoords, depth);

  float3 normal = normalize(NormalTexture.Sample(VertexCoords).xyz * 2.0 - 1.0);

  // metallic, perceptual roughness, reflectance, stencil
  MaterialChannel material =
      MaterialChannel(MaterialTexture.Sample(VertexCoords));
  float3 albedo = AlbedoTexture.Sample(VertexCoords).rgb;

  float3 viewDir = normalize(worldPosition - CameraData[0].Position);
  float3 f0 = ComputeFresnel0(albedo, material.metallic, material.reflectance);
  float3 diffuse = ComputeDiffuseColor(albedo, material.metallic);
  float NoV = ClampNoV(dot(normal, -viewDir));
  float3 E = prefilteredDFG(material.perceptualRoughness, f0, NoV);
  float ao = AmbientOcclusionTexture.Sample(VertexCoords).x;

  float3 reflectedRay = reflect(viewDir, normal);

  LightProbeSampleInfo probeInfo = SampleLightProbes(
      worldPosition, normal, reflectedRay, material.perceptualRoughness);

  if (probeInfo.influence >= 1.0) {
    probeInfo.radiance /= probeInfo.influence;
    probeInfo.irradiance /= probeInfo.influence;
  } else // need skybox influence
  {
    float skyboxInfluence = 1.0 - probeInfo.influence;

    float3 reflected =
        prefilteredRadiance(SpecularTextures, float4(reflectedRay, 0.0),
                            material.perceptualRoughness);
    probeInfo.radiance = reflected * skyboxInfluence + probeInfo.radiance;

    float3 irradiance =
        prefilteredIrradiance(IrradianceTextures, float4(normal, 0.0));

    probeInfo.irradiance += irradiance * skyboxInfluence;
  }

  probeInfo.irradiance *= INV_PI;

  if (!SpecularEnabled || false) {
    probeInfo.radiance *= 0.0;
    probeInfo.irradiance *=
        (1.0 - E) *
        albedo; // since we turn off specular, fully metallic objects will be black
    // to solve this, we multiply by albedo instead of diffuse
  } else {
    probeInfo.radiance *= E;
    probeInfo.irradiance *= (1.0 - E) * diffuse;
  }

  probeInfo.radiance *= SpecularOcclusion(NoV, ao, material.roughness);
  probeInfo.irradiance *= ao;

  // Radiance = probeInfo.radiance;
  // Irradiance = probeInfo.irradiance;
  FragmentOutput output;
  output.Radiance = float4(probeInfo.radiance, 1.0f);
  output.Irradiance = float4(probeInfo.irradiance, 1.0f);

  return output;
}
