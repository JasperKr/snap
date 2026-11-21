import camera;
import material;
import Indirect.drawlists;
import transform;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float2 tex_coord : TEXCOORDS;
};

uniform RWStructuredBuffer<DrawItem>.Handle DrawItems;

[shader("vertex")]
VertexStageOutput vertexMain(uint instance: SV_InstanceID, uint vertexID: SV_VertexID) {
  VertexStageOutput out;
  DrawItem item = DrawItems[instance];

  Vertex vertex = item.Vertices[vertexID];
  float4 worldPos = mul(float4(vertex.Position, 1.0), item.GetTransform());

  out.sv_position = mul(CameraData[0].ViewProjectionMatrix, worldPos);
  out.tex_coord = vertex.TexCoords;

  return out;
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
