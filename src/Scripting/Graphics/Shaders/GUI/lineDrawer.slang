struct VertexInput {
  float3 start : POSITION;
  float3 end : END_POSITION;
  float4 color : COLOR;
  float thickness : THICKNESS;
};

struct VertexOutput {
  float4 position : SV_POSITION;
  float4 color : COLOR;
  float2 tex_coord : TEXCOORDS;
};

import utils;
import camera;

uniform uint2 viewportSize;

static const float2 corners[6] = { { 0, 1 }, { 1, 1 }, { 1, 0 },
                                   { 0, 1 }, { 1, 0 }, { 0, 0 } };

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint vertexID: SV_VertexID) {
  VertexOutput output;

  float4 clipStart =
      mul(CameraData[0].ViewProjectionMatrix, float4(input.start, 1.0));
  float4 clipEnd =
      mul(CameraData[0].ViewProjectionMatrix, float4(input.end, 1.0));

  // Only use NDC for screen-space calculations.
  float3 ndcStart = clipStart.xyz / clipStart.w;
  float3 ndcEnd = clipEnd.xyz / clipEnd.w;

  float2 dir = normalize(ndcStart.xy - ndcEnd.xy);

  float2 texCoord = corners[vertexID % 6];

  // Offset in NDC.
  float2 offset =
      float2(-dir.y, dir.x) * (input.thickness) / float2(viewportSize);

  // Interpolate in clip space.
  float4 clipPos = lerp(clipStart, clipEnd, texCoord.x);

  // Convert NDC offset back to clip space.
  clipPos.xy += offset * (1.0 - 2.0 * texCoord.y) * clipPos.w;

  output.position = clipPos;
  output.color = input.color;
  output.tex_coord = texCoord;

  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) {
  float4 color = input.color;

  float dist = (0.5 - input.tex_coord.y) * 2.0;
  float alpha = smoothstep(1.0, 0.0, abs(dist));

  color.w *= alpha;
  return color;
}
