import camera;
import Indirect.drawlists;
import transform;
import Indirect.meshletPayload;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
};

uniform StructuredBuffer<DrawItem>.Handle DrawItems;

[shader("mesh")]
[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void meshMain(uint instance: SV_GroupThreadID, uint group: SV_GroupID, in payload TaskPayload payload, out vertices VertexStageOutput vertices[64],
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
    vertices[instance] = outVtx;
  }

  if (instance < meshlet.PrimitiveCount) {
    triangles[instance] = meshlet.LocalIndices[instance];
  }
}

[shader("fragment")]
void fragmentMain() {}
