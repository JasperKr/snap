import camera;
import material;
import Indirect.drawlists;
import transform;
import Indirect.meshletPayload;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float2 tex_coord : TEXCOORDS;
};

uniform StructuredBuffer<DrawItem>.Handle DrawItems;

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void meshMain(in payload TaskPayload payload, uint instance: SV_GroupThreadID, uint group: SV_GroupID, out vertices VertexStageOutput vertices[64],
              out indices uint3 triangles[126]) {

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
    outVtx.tex_coord = vertex.TexCoords;
    vertices[instance] = outVtx;
  }

  if (instance < meshlet.PrimitiveCount) {
    triangles[instance] = meshlet.LocalIndices[instance];
  }
}

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

[shader("fragment")]
void fragmentMain(FragmentInput input, uint instance: SV_InstanceID) {
  MaterialData matData = DrawItems[instance].GetMaterial();
  float4 texColor = matData.AlbedoOrDefault(input.tex_coord);

  float alpha = texColor.a * matData.albedoFactor.a;

  // No need to check if material is alpha masked
  // Since this shader is only used for alpha masked materials
  if (alpha <= matData.alphaCutoff) {
    discard;
  }
}
