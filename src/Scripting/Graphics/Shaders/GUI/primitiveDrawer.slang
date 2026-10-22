struct VertexInput {
  float3 position : POSITION;
  float4 color : COLOR;
};

struct VertexOutput {
  float4 position : SV_POSITION;
  float4 color : COLOR;
};

import utils;
import camera;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
  VertexOutput output;
  output.position =
      mul(CameraData[0].ViewProjectionMatrix, float4(input.position, 1.0));
  output.color = input.color;
  return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) { return input.color; }
