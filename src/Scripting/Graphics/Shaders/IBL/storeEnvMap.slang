uniform SamplerCube.Handle Input;

uniform WTexture2DArray<float4>.Handle Output_Mip_0;
uniform WTexture2DArray<float4>.Handle Output_Mip_1;
uniform WTexture2DArray<float4>.Handle Output_Mip_2;
uniform WTexture2DArray<float4>.Handle Output_Mip_3;
uniform WTexture2DArray<float4>.Handle Output_Mip_4;
uniform WTexture2DArray<float4>.Handle Output_Mip_5;
uniform WTexture2DArray<float4>.Handle Output_Mip_6;

float signNotZero(float x) { return x < 0.0 ? -1.0 : 1.0; }

float2 signNotZero(float2 v) {
  return float2(signNotZero(v.x), signNotZero(v.y));
}

float3 DecodeOctahedron(float2 e) {
  e = e * 2.0 - 1.0;
  float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z <= 0.0)
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

import cubemapHelpers;

uniform int Slice;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  int level;
  float2 uv;

  getTexelInfo(id, level, uv, 512);

  if (level < 0)
    return;

  uv.y = -uv.y;
  float3 direction = DecodeOctahedron(uv * 0.5 + 0.5);

  direction.z = -direction.z;

  float4 data = Input.SampleLevel(direction, float(level));

  switch (level) {
  case 0:
    Output_Mip_0.Store(int3(id.xy, Slice), data);
    break;
  case 1:
    Output_Mip_1.Store(int3(id.xy, Slice), data);
    break;
  case 2:
    Output_Mip_2.Store(int3(id.xy, Slice), data);
    break;
  case 3:
    Output_Mip_3.Store(int3(id.xy, Slice), data);
    break;
  case 4:
    Output_Mip_4.Store(int3(id.xy, Slice), data);
    break;
  case 5:
    Output_Mip_5.Store(int3(id.xy, Slice), data);
    break;
  default:
    Output_Mip_6.Store(int3(id.xy, Slice), data);
    break;
  }
}
