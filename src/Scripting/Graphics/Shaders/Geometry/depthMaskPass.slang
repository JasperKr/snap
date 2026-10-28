import camera;
import material;

import transform;

uniform uint MaterialBufferIndex;
uniform uint ModelTransformIndex;

struct VertexStageOutput {
  float4 sv_position : SV_Position;
  float2 tex_coord : TEXCOORDS;
};

struct VertexInput {
  float3 Position : POSITION;
  float2 TexCoords : TEXCOORD;
  // uint Normal : NORMAL;
  // uint Tangent : TANGENT;
  // float4 Color : COLOR;
};

uniform uint2 testPadding;

uniform Sampler2D.Handle AlbedoTexture;

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
  float4x4 transform = GetModelMatrix(ModelTransformIndex);
  float4 worldPos = mul(float4(input.Position, 1.0), transform);

  VertexStageOutput out;
  out.sv_position = mul(CameraData[0].ViewProjectionMatrix, worldPos);
  out.tex_coord = input.TexCoords;

  return out;
}

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

[shader("fragment")]
void fragmentMain(FragmentInput input) {
  float4 texColor = AlbedoTexture.Sample(input.tex_coord);
  MaterialData matData = MaterialBuffer[MaterialBufferIndex];

  float alpha = texColor.a * matData.albedoFactor.a;

  // No need to check if material is alpha masked
  // Since this shader is only used for alpha masked materials
  if (alpha <= matData.alphaCutoff) {
    discard;
  }
}
