import camera;
import material;
import utils;
import Lighting.pixel;
import Lighting.physicallyBased;
import constants;
import Lighting.Lights.directionalLight;
import transform;

import Indirect.drawlists;
import Indirect.meshletPayload;

uniform uint FrameRandomState;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  float4 currentClip : CURRENT_CLIP;
  float4 previousClip : PREV_CLIP;
  uint meshIndex : MESHIDX;
};

uniform StructuredBuffer<DrawItem>.Handle DrawItems;

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void meshMain(uint instance: SV_GroupThreadID, uint group: SV_GroupID, in payload TaskPayload payload, out vertices VertexStageOutput vertices[64],
              out indices uint3 triangles[126], out primitives uint primdata[126]) {

  uint meshletIndex = payload.MeshletOffset[group];

  if (meshletIndex >= payload.MeshletCount) {
    return;
  }

  uint meshIdx = payload.MeshIndices[group];
  DrawItem item = DrawItems[meshIdx];
  Meshlet meshlet = item.Meshlets[meshletIndex];

  SetMeshOutputCounts(meshlet.VertexCount, meshlet.PrimitiveCount);

  Transform transform = GetModelTransform(item.TransformIndex);

  if (instance < meshlet.VertexCount) {
    uint vertexIndex = meshlet.Indices[instance];
    Vertex vertex = item.Vertices[vertexIndex];

    VertexStageOutput outVtx;

    float4 worldPos = mul(float4(vertex.Position, 1.0), transform.ModelMatrix);
    outVtx.sv_position = mul(CameraData[0].ViewProjectionMatrix, worldPos);
    outVtx.currentClip = outVtx.sv_position;
    outVtx.previousClip = mul(CameraData[1].ViewProjectionMatrix, worldPos);

    float3x3 NormalMatrix = float3x3(transform.NormalMatrix);

    outVtx.tex_coord = vertex.TexCoords;
    outVtx.normal = normalize(mul(NormalMatrix, vertex.GetNormal()).xyz);

    float4 tangent = vertex.GetTangent();
    outVtx.tangent.xyz = normalize(mul(NormalMatrix, tangent.xyz));
    outVtx.tangent.w = tangent.w;

    outVtx.color = vertex.GetColor();
    outVtx.meshIndex = meshIdx;

    vertices[instance] = outVtx;
  }

  if (instance < meshlet.PrimitiveCount) {
    triangles[instance] = meshlet.LocalIndices[instance];
  }
}

struct FragmentInput {
  float3 WorldPosition : WORLDPOSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  bool isFrontFace : SV_IsFrontFace;
  float4 screenCoords : SV_Position;
  uint meshIndex : MESHIDX;
};

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float3 normal = normalize(input.normal);
  float4 tangent = float4(normalize(input.tangent.xyz), input.tangent.w);

  if (!input.isFrontFace) {
    normal = -normal;
    tangent.xyz = -tangent.xyz;
  }

  DrawItem item = DrawItems[input.meshIndex];

  MaterialData matData = item.GetMaterial();
  float4 texColor = matData.AlbedoOrDefault(input.tex_coord);

  float alpha = texColor.a * matData.albedoFactor.a;

  float3 albedo = texColor.rgb * matData.albedoFactor.rgb;

  float2 metallicRoughness = matData.MetallicRoughnessOrDefault(input.tex_coord);

  float3 normalMap = normalize(matData.NormalOrDefault(input.tex_coord) * 2.0f - 1.0f);

  float3 bitangent = cross(normal, tangent.xyz) * tangent.w;
  float3x3 TBN = float3x3(tangent.xyz, bitangent, normal);
  TBN = transpose(TBN);
  normal = mul(TBN, normalMap);

  float3 emissiveSample = matData.EmissiveOrDefault(input.tex_coord);
  float reflectanceSample = matData.ReflectanceOrDefault(input.tex_coord);

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

  float3 E = prefilteredDFG(pixel.PerceptualRoughness, pixel.Fresnel0, pixel.NoV);
  float3 diffuse = ComputeDiffuseColor(pixel.Albedo, pixel.Metallic);

  float3 irradiance = float3(0.5, 0.5, 0.5); // Some ambient lighting
  irradiance *= INV_PI;

  float3 radiance = irradiance; // Specular IBL contribution
  irradiance *= (1.0f.xxx - E) * diffuse;
  radiance *= E;

  float3 color = direct + irradiance + radiance + emissive;

  return float4(color * alpha, alpha);
}
