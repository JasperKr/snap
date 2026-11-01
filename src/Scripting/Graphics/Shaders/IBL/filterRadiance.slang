
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

uniform SamplerCube.Handle InputTexture;

uniform WTexture2DArray<float4>.Handle OutputTexture0;
uniform WTexture2DArray<float4>.Handle OutputTexture1;
uniform WTexture2DArray<float4>.Handle OutputTexture2;
uniform WTexture2DArray<float4>.Handle OutputTexture3;
uniform WTexture2DArray<float4>.Handle OutputTexture4;
uniform WTexture2DArray<float4>.Handle OutputTexture5;
uniform WTexture2DArray<float4>.Handle OutputTexture6;

import utils;

uniform StructuredBuffer<float4>.Handle Coefficients;

#define NUM_TAPS 32

import helpers;
import cubemapHelpers;

#define GROUP_SIZE 64

static float3[3] UpAxes = { float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
                            float3(0.0, 0.0, 1.0) };

[shader("compute")]
[numthreads(GROUP_SIZE, 1, 1)]
void computeMain(uint3 invocationID: SV_DispatchThreadID) {
  int3 id = int3(invocationID);

  // INPUT:
  // id.x = the linear address of the texel (ignoring face)
  // id.y = the face
  // -> use to index output texture
  // id.x = texel x
  // id.y = texel y
  // id.z = face

  int level;
  float2 uv;

  getTexelInfo(id, level, uv, 128);

  if (level < 0)
    return;

  // determine dir / pos for the texel
  float3 dir, adir, frameZ;
  {
    get_dir(dir, uv, id.z);
    frameZ = normalize(dir);
    adir = abs(dir);
  }

  // GGX gather colors
  float4 color = float4(0.0);

  [[loop]]
  for (int axis = 0; axis < 3; axis++) {
    const int otherAxis0 = 1 - (axis & 1) - (axis >> 1);
    const int otherAxis1 = 2 - (axis >> 1);

    float frameweight = (max(adir[otherAxis0], adir[otherAxis1]) - .75) / .25;

    if (frameweight > 0) {
      // determine frame
      float3 UpVector = UpAxes[axis];
      float3 frameX = normalize(cross(UpVector, frameZ));
      float3 frameY = cross(frameZ, frameX);

      // calculate parametrization for polynomial
      float Nx = dir[otherAxis0];
      float Ny = dir[otherAxis1];
      float Nz = adir[axis];

      float NmaxXY = max(abs(Ny), abs(Nx));
      Nx /= NmaxXY;
      Ny /= NmaxXY;

      float theta;
      if (Ny < Nx)
        if (Ny <= -0.999)
          theta = Nx;
        else
          theta = Ny;
      else if (Ny >= 0.999)
        theta = -Nx;
      else
        theta = -Ny;

      float phi;
      if (Nz <= -0.999)
        phi = -NmaxXY;
      else if (Nz >= 0.999)
        phi = NmaxXY;
      else
        phi = Nz;

      float theta2 = theta * theta;
      float phi2 = phi * phi;

      // sample
      [[loop]]
      for (int iSuperTap = 0; iSuperTap < NUM_TAPS / 4; iSuperTap++) {
        const int index = (NUM_TAPS / 4) * axis + iSuperTap;
        float4 coeffsDir0[3];
        float4 coeffsDir1[3];
        float4 coeffsDir2[3];
        float4 coeffsLevel[3];
        float4 coeffsWeight[3];

        [[unroll]]
        for (int iCoeff = 0; iCoeff < 3; iCoeff++) {
          // Array: float4 coeffs[7][5][3][24]
          // indexed as:
          // level * 5 * 3 * 24 +
          // type * 3 * 24 +
          // coeff * 24 +
          // index
          // where the indexed value is a float4

          int finalIndex = level * 5 * 3 * 24 + iCoeff * 24 + index;

          coeffsDir0[iCoeff] = Coefficients[finalIndex + (0 * 24 * 3)];
          coeffsDir1[iCoeff] = Coefficients[finalIndex + (1 * 24 * 3)];
          coeffsDir2[iCoeff] = Coefficients[finalIndex + (2 * 24 * 3)];
          coeffsLevel[iCoeff] = Coefficients[finalIndex + (3 * 24 * 3)];
          coeffsWeight[iCoeff] = Coefficients[finalIndex + (4 * 24 * 3)];
        }

        [[unroll]]
        for (int iSubTap = 0; iSubTap < 4; iSubTap++) {
          // determine sample attributes (dir, weight, level)
          float3 sample_dir = frameX * (coeffsDir0[0][iSubTap] +
                                        coeffsDir0[1][iSubTap] * theta2 +
                                        coeffsDir0[2][iSubTap] * phi2) +
                              frameY * (coeffsDir1[0][iSubTap] +
                                        coeffsDir1[1][iSubTap] * theta2 +
                                        coeffsDir1[2][iSubTap] * phi2) +
                              frameZ * (coeffsDir2[0][iSubTap] +
                                        coeffsDir2[1][iSubTap] * theta2 +
                                        coeffsDir2[2][iSubTap] * phi2);

          float sample_level = coeffsLevel[0][iSubTap] +
                               coeffsLevel[1][iSubTap] * theta2 +
                               coeffsLevel[2][iSubTap] * phi2;

          float sample_weight = coeffsWeight[0][iSubTap] +
                                coeffsWeight[1][iSubTap] * theta2 +
                                coeffsWeight[2][iSubTap] * phi2;
          sample_weight *= frameweight;

          // adjust for jacobian
          sample_dir /= max(abs(sample_dir[0]),
                            max(abs(sample_dir[1]), abs(sample_dir[2])));
          sample_level += 0.75 * log2(dot(sample_dir, sample_dir));

          // sample cubemap
          color.xyz +=
              Sanitise(InputTexture.SampleLevel(sample_dir, sample_level).xyz) *
              sample_weight;
          color.w += sample_weight;
        }
      }
    }
  }

  color /= max(color.w, 0.0001);

  // write color
  color = max(color, float4(0.0));
  color.w = 1.0;

  switch (level) {
  case 0:
    OutputTexture0.Store(id, color);
    break;
  case 1:
    OutputTexture1.Store(id, color);
    break;
  case 2:
    OutputTexture2.Store(id, color);
    break;
  case 3:
    OutputTexture3.Store(id, color);
    break;
  case 4:
    OutputTexture4.Store(id, color);
    break;
  case 5:
    OutputTexture5.Store(id, color);
    break;
  default:
    OutputTexture6.Store(id, color);
    break;
  }
}
