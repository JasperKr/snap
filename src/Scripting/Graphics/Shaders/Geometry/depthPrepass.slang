import camera;

import transform;
struct PushConstantBuffer {
  uint ModelTransformIndex;
};

[[vk::push_constant]]
PushConstantBuffer PushConstants;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
};

struct VertexInput {
  float3 Position : POSITION;
  // float2 TexCoords : TEXCOORD;
  // uint Normal : NORMAL;
  // uint Tangent : TANGENT;
  // float4 Color : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  float4x4 transform = GetModelMatrix(PushConstants.ModelTransformIndex);
  float4 worldPos = mul(float4(input.Position, 1.0), transform);

  VertexStageOutput out;
  out.sv_position = mul(camera.ViewProjectionMatrix, worldPos);
  return out;
}

[shader("fragment")]
void fragmentMain() {}
