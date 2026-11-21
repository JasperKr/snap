import camera;
import Indirect.drawlists;
import transform;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
};

uniform RWStructuredBuffer<DrawItem>.Handle DrawItems;

[shader("vertex")]
VertexStageOutput vertexMain(uint instance: SV_InstanceID, uint vertexID: SV_VertexID) {
  VertexStageOutput out;

  DrawItem item = DrawItems[instance];
  Vertex vertex = item.Vertices[vertexID];

  float4 worldPos = mul(float4(vertex.Position, 1.0), item.GetTransform());
  out.sv_position = mul(CameraData[0].ViewProjectionMatrix, worldPos);

  return out;
}

[shader("fragment")]
void fragmentMain() {}
