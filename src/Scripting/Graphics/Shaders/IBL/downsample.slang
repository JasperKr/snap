// Copyright 2016 Activision Publishing, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

uniform SamplerCube.Handle tex_hi_res;
// uniform highp samplerCube tex_hi_res;
uniform WTexture2DArray<float4>.Handle tex_lo_res;

// layout(r11f_g11f_b10f) uniform restrict writeonly imageCube tex_lo_res;

// SamplerState bilinear : register( s0 );

import utils;

void get_dir_0(out float3 dir, in float u, in float v) {
  dir[0] = 1;
  dir[1] = v;
  dir[2] = -u;
}
void get_dir_1(out float3 dir, in float u, in float v) {
  dir[0] = -1;
  dir[1] = v;
  dir[2] = u;
}
void get_dir_2(out float3 dir, in float u, in float v) {
  dir[0] = u;
  dir[1] = 1;
  dir[2] = -v;
}
void get_dir_3(out float3 dir, in float u, in float v) {
  dir[0] = u;
  dir[1] = -1;
  dir[2] = v;
}
void get_dir_4(out float3 dir, in float u, in float v) {
  dir[0] = u;
  dir[1] = v;
  dir[2] = 1;
}
void get_dir_5(out float3 dir, in float u, in float v) {
  dir[0] = -u;
  dir[1] = v;
  dir[2] = -1;
}

void get_dir(out float3 dir, in float u, in float v, in int face) {
  switch (face) {
  case 0:
    get_dir_0(dir, u, v);
    break;
  case 1:
    get_dir_1(dir, u, v);
    break;
  case 2:
    get_dir_2(dir, u, v);
    break;
  case 3:
    get_dir_3(dir, u, v);
    break;
  case 4:
    get_dir_4(dir, u, v);
    break;
  default:
    get_dir_5(dir, u, v);
    break;
  }
}

float calcWeight(float u, float v) {
  float val = u * u + v * v + 1.0;
  return val * sqrt(val);
}

float3 Sample(float3 dir) {
  return Sanitise(tex_hi_res.SampleLevelZero(dir).xyz);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  uint res_lo;
  {
    uint h, e;
    tex_lo_res.GetDimensions(res_lo, h, e);
    // res_lo = uint(imageSize(tex_lo_res).x);
  }

  if (id.x < res_lo && id.y < res_lo) {
    float inv_res_lo = 1.0 / float(res_lo);

    float u0 = (float(id.x) * 2.0 + 1.0 - 0.75) * inv_res_lo - 1.0;
    float u1 = (float(id.x) * 2.0 + 1.0 + 0.75) * inv_res_lo - 1.0;

    float v0 = (float(id.y) * 2.0 + 1.0 - 0.75) * -inv_res_lo + 1.0;
    float v1 = (float(id.y) * 2.0 + 1.0 + 0.75) * -inv_res_lo + 1.0;

    float4 weights;
    weights.x = calcWeight(u0, v0);
    weights.y = calcWeight(u1, v0);
    weights.z = calcWeight(u0, v1);
    weights.w = calcWeight(u1, v1);

    const float wsum = 0.5 / (weights.x + weights.y + weights.z + weights.w);

    weights = weights * wsum + 0.125;

#if 1
    float3 dir;
    float3 color;
    switch (id.z) {
    case 0:
      get_dir_0(dir, u0, v0);
      color = Sample(dir) * weights.x;

      get_dir_0(dir, u1, v0);
      color += Sample(dir) * weights.y;

      get_dir_0(dir, u0, v1);
      color += Sample(dir) * weights.z;

      get_dir_0(dir, u1, v1);
      color += Sample(dir) * weights.w;
      break;
    case 1:
      get_dir_1(dir, u0, v0);
      color = Sample(dir) * weights.x;

      get_dir_1(dir, u1, v0);
      color += Sample(dir) * weights.y;

      get_dir_1(dir, u0, v1);
      color += Sample(dir) * weights.z;

      get_dir_1(dir, u1, v1);
      color += Sample(dir) * weights.w;
      break;
    case 2:
      get_dir_2(dir, u0, v0);
      color = Sample(dir) * weights.x;

      get_dir_2(dir, u1, v0);
      color += Sample(dir) * weights.y;

      get_dir_2(dir, u0, v1);
      color += Sample(dir) * weights.z;

      get_dir_2(dir, u1, v1);
      color += Sample(dir) * weights.w;
      break;
    case 3:
      get_dir_3(dir, u0, v0);
      color = Sample(dir) * weights.x;

      get_dir_3(dir, u1, v0);
      color += Sample(dir) * weights.y;

      get_dir_3(dir, u0, v1);
      color += Sample(dir) * weights.z;

      get_dir_3(dir, u1, v1);
      color += Sample(dir) * weights.w;
      break;
    case 4:
      get_dir_4(dir, u0, v0);
      color = Sample(dir) * weights.x;

      get_dir_4(dir, u1, v0);
      color += Sample(dir) * weights.y;

      get_dir_4(dir, u0, v1);
      color += Sample(dir) * weights.z;

      get_dir_4(dir, u1, v1);
      color += Sample(dir) * weights.w;
      break;
    default:
      get_dir_5(dir, u0, v0);
      color = Sample(dir) * weights.x;

      get_dir_5(dir, u1, v0);
      color += Sample(dir) * weights.y;

      get_dir_5(dir, u0, v1);
      color += Sample(dir) * weights.z;

      get_dir_5(dir, u1, v1);
      color += Sample(dir) * weights.w;
      break;
    }
#else
    float3 dir;
    get_dir(dir, u0, v0, id.z);
    float4 color = Sample(dir) * weights.x;

    get_dir(dir, u1, v0, id.z);
    color += Sample(dir) * weights.y;

    get_dir(dir, u0, v1, id.z);
    color += Sample(dir) * weights.z;

    get_dir(dir, u1, v1, id.z);
    color += Sample(dir) * weights.w;
#endif

    const float Clamped = 100.0;

    if (isinf(color.r)) {
      color.r = Clamped;
    }
    if (isinf(color.g)) {
      color.g = Clamped;
    }
    if (isinf(color.b)) {
      color.b = Clamped;
    }

    float luma = Luminance(color.rgb);
    if (luma > Clamped) {
      color.rgb *= Clamped / luma;
    }

    tex_lo_res.Store(int3(id), float4(color.rgb, 1.0));
  }
}
