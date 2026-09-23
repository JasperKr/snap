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

float GammaToLinear(float gamma) {
  return gamma <= 0.04045 ? gamma / 12.92 : pow((gamma + 0.055) / 1.055, 2.4);
}

float3 GammaToLinear(float3 gamma) {
  return float3(GammaToLinear(gamma.r), GammaToLinear(gamma.g),
                GammaToLinear(gamma.b));
}

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float4 color = MainTexture.Sample(input.tex_coord);
  color *= input.color;
  // color.rgb *= color.a;

#ifdef __COLORSPACE_GAMMA_CORRECT
  // color.rgb = GammaToLinear(color.rgb);
  // color.a = 1.0 - GammaToLinear(1.0 - color.a);
#endif

  return color;
}
