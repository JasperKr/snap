struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float2 tex_coord : TEXCOORD0;
  float4 color : COLOR;
};

struct VertexInput {
  float2 Position : POSITION;
  float2 TexCoord : TEXCOORD0;
  float4 Color : COLOR;
};

struct PushConstantsBuffer {
  float4x4 ProjectionMatrix;
};

[[vk::push_constant]]
PushConstantsBuffer PushConstants;

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  VertexStageOutput out;
  out.sv_position =
      mul(float4(input.Position, 0.0, 1.0), PushConstants.ProjectionMatrix);
  out.tex_coord = input.TexCoord;
  out.color = input.Color;
  return out;
}

struct FragmentInput {
  float2 tex_coord : TEXCOORD0;
  float4 color : COLOR;
};

Sampler2D MainTexture;

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  return float4(float3(1.0), MainTexture.Sample(input.tex_coord).r) *
         input.color;
}
