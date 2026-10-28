import camera;
import material;
import utils;
import Lighting.pixel;
import Lighting.physicallyBased;
import constants;

import Lighting.Lights.directionalLight;

import transform;

uniform uint MaterialBufferIndex;
uniform uint ModelTransformIndex;
uniform uint FrameRandomState;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float3 WorldPosition : WORLDPOSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
};

struct VertexInput {
  float3 Position : POSITION;
  float2 TexCoords : TEXCOORD;
  uint Normal : NORMAL;
  uint Tangent : TANGENT;
  float4 Color : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;

  Transform transform = GetModelTransform(ModelTransformIndex);

  float4 worldPos = mul(float4(input.Position, 1.0), transform.ModelMatrix);
  out.sv_position = mul(CameraData[0].ViewProjectionMatrix, worldPos);

  float3x3 NormalMatrix = float3x3(transform.NormalMatrix);

  out.tex_coord = input.TexCoords;
  float3 normal = UnpackNormal(input.Normal).xyz;
  out.normal = normalize(mul(NormalMatrix, normal).xyz);

  float4 tangent = UnpackTangent(input.Tangent);
  out.tangent.xyz = normalize(mul(NormalMatrix, tangent.xyz));
  out.tangent.w = tangent.w;

  out.color = input.Color;
  out.WorldPosition = worldPos.xyz;
  return out;
}

struct FragmentInput {
  float3 WorldPosition : WORLDPOSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  bool isFrontFace : SV_IsFrontFace;
  float4 screenCoords : SV_Position;
};

uniform Sampler2D.Handle AlbedoTexture;
uniform Sampler2D.Handle MetallicRoughnessTexture;
uniform Sampler2D.Handle NormalTexture;
uniform Sampler2D.Handle EmissiveTexture;
uniform Sampler2D.Handle ReflectanceTexture;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float3 normal = normalize(input.normal);
  float4 tangent = float4(normalize(input.tangent.xyz), input.tangent.w);

  if (!input.isFrontFace) {
    normal = -normal;
    tangent.xyz = -tangent.xyz;
  }

  float4 texColor = AlbedoTexture.Sample(input.tex_coord);
  MaterialData matData = MaterialBuffer[MaterialBufferIndex];

  float alpha = texColor.a * matData.albedoFactor.a;

  float3 albedo = texColor.rgb * matData.albedoFactor.rgb;

  float2 metallicRoughness =
      MetallicRoughnessTexture.Sample(input.tex_coord).bg;

  float3 normalMap =
      normalize(NormalTexture.Sample(input.tex_coord).xyz * 2.0f - 1.0f);

  float3 bitangent = cross(normal, tangent.xyz) * tangent.w;
  float3x3 TBN = float3x3(tangent.xyz, bitangent, normal);
  TBN = transpose(TBN);
  normal = mul(TBN, normalMap);

  float3 emissiveSample = EmissiveTexture.Sample(input.tex_coord).rgb;
  float reflectanceSample = ReflectanceTexture.Sample(input.tex_coord).r;

  float metallic = metallicRoughness.x * matData.metallicFactor;
  float perceptualRoughness = metallicRoughness.y * matData.roughnessFactor;
  float reflectance = reflectanceSample * matData.reflectanceFactor;
  float3 emissive = emissiveSample * matData.emissiveFactor;

  Pixel pixel;
  pixel.Normal = normal;
  pixel.Albedo = albedo;
  pixel.Metallic = metallic;
  pixel.Roughness = CalculateRoughness(perceptualRoughness);
  pixel.PerceptualRoughness = perceptualRoughness;
  pixel.Fresnel0 = ComputeFresnel0(albedo, metallic, reflectance);

  float3 viewDir = normalize(CameraData[0].Position - input.WorldPosition);

  pixel.NoV = ClampNoV(dot(normal, viewDir));
  pixel.LambdaV = ComputeLambdaV(pixel.Roughness, pixel.NoV);

  uint2 screenCoords = uint2(input.screenCoords.xy);
  float3 direct = float3(0.0);

  for (int i = 0; i < DirectionalLightCount; i++) {
    var light = DirectionalLights.Load(i);

    direct += light.GetIntensityAt(input.WorldPosition, pixel, viewDir);
  }

  float3 E =
      prefilteredDFG(pixel.PerceptualRoughness, pixel.Fresnel0, pixel.NoV);
  float3 diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

  float3 irradiance = float3(0.5, 0.5, 0.5); // Some ambient lighting
  irradiance *= INV_PI;

  float3 radiance = irradiance; // Specular IBL contribution
  irradiance *= (1.0f.xxx - E) * diffuse;
  radiance *= E;

  float3 color = direct + irradiance + radiance + emissive;

  return float4(color * alpha, alpha);
}
