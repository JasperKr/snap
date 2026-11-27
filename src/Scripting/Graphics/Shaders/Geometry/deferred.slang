import camera;
import material;
import utils;
import Lighting.pixel;
import Lighting.physicallyBased;
import constants;
import Indirect.drawlists;
import transform;

import Indirect.meshletPayload;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  float4 currentClip : CURRENT_CLIP;
  float4 previousClip : PREV_CLIP;
};

struct PrimitiveAttribute {
  uint meshIndex : MESHIDX;
};

uniform StructuredBuffer<DrawItem>.Handle DrawItems;

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void meshMain(in payload TaskPayload payload, uint instance: SV_GroupThreadID, uint group: SV_GroupID, out vertices VertexStageOutput vertices[64],
              out indices uint3 triangles[126], out primitives PrimitiveAttribute primdata[126]) {

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

    vertices[instance] = outVtx;
  }

  if (instance < meshlet.PrimitiveCount) {
    triangles[instance] = meshlet.LocalIndices[instance];
    primdata[instance] = PrimitiveAttribute(meshletIndex);
  }
}

struct FragmentInput {
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 tex_coord : TEXCOORDS;
  float4 color : COLOR;
  float4 currentClip : CURRENT_CLIP;
  float4 previousClip : PREV_CLIP;
  bool isFrontFace : SV_IsFrontFace;
  uint meshIndex : MESHIDX;
};

struct FragmentOutput {
  float4 Albedo : SV_Target0;
  float4 Normal : SV_Target1;
  float4 Material : SV_Target2;
  float4 Emissive : SV_Target3;
  float2 Motion : SV_Target4;
}

[shader("fragment")]
FragmentOutput fragmentMain(FragmentInput input) {
  float3 normal = normalize(input.normal);
  float4 tangent = float4(normalize(input.tangent.xyz), input.tangent.w);

  if (!input.isFrontFace) {
    normal = -normal;
    tangent = -tangent;
  }

  MaterialData matData = DrawItems[input.meshIndex].GetMaterial();
  float4 texColor = matData.AlbedoOrDefault(input.tex_coord);

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

  MaterialChannel material = MaterialChannel(metallic, perceptualRoughness, reflectance, 1.0F);

  float3 emissive = emissiveSample * matData.emissiveFactor;

  float2 currentNDC = input.currentClip.xy / input.currentClip.w;
  float2 previousNDC = input.previousClip.xy / input.previousClip.w;

  currentNDC = currentNDC * 0.5 + 0.5;   // remap to [0, 1] range
  previousNDC = previousNDC * 0.5 + 0.5; // remap to [0, 1] range

  float2 motion = currentNDC - previousNDC;
  // motion = motion * 0.5 + 0.5; // remap to [0, 1] range

  if (CameraData[0].HistoryInvalidated()) {
    motion = float2(0.0f);
  }

  return FragmentOutput(float4(albedo, 0.0), float4(normalize(normal) * 0.5 + 0.5, 0.0), material.packed, float4(emissive, 1.0), PackMotionVector(motion));
}
