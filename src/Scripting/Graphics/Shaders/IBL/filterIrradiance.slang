// #include "constants.glsl"
// #include "cubemapFilter.glsl"

import helpers;
import cubemapHelpers;
import constants;

uniform int Slice;

// uniform highp sampler2DArray RadianceInput;
// layout(r11f_g11f_b10f) uniform restrict writeonly image2DArray IrradianceOutput;

uniform Sampler2DArray.Handle RadianceInput;
uniform WTexture2DArray<float4>.Handle IrradianceOutput;

/*

int phiIndex = 0;

  for (float phi = 0.0F; phi < 2.0F * std::numbers::pi; phi += sampleDelta) {
    // float yawOffset = (phiIndex & 1) ? sampleDelta * 0.5F : 0.0F;
    float yawOffset = float(phiIndex % 16) * (sampleDelta * 0.0625F);

    float thetaStart = yawOffset;

    for (float theta = thetaStart; theta < (0.5F * std::numbers::pi);
         theta += sampleDelta) {

      float t = theta / (0.5 * std::numbers::pi);
      t = std::sqrt(t);

      float biasedTheta = t * (0.5 * std::numbers::pi);

      float sinTheta = std::sin(biasedTheta);
      float cosTheta = std::cos(biasedTheta);

      float sinPhi = std::sin(phi);
      float cosPhi = std::cos(phi);

      Math::Vec3 sampleVec = right * (sinTheta * cosPhi) +
                             upVect * (sinTheta * sinPhi) + normal * cosTheta;

      sampleVec = sampleVec.Normalize();

      lineDrawer.OverlayLine(sampleVec * 0.5F, sampleVec,
                             Math::Vec4(1.0F, 1.0F, 1.0F, 1.0F), 2.0F);
    }

    phiIndex++;
  }

*/

float3 calculateIrradiance(const float3 normal) {
  float3 irradiance = float3(0.0);

  float3 up = float3(0.0, 1.0, 0.0);
  float3 right = normalize(cross(up, normal));
  up = normalize(cross(normal, right));

  float sampleDelta = PI / 16.0;
  float nrSamples = 0.0;

  int phiIndex = 0;

  for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
    float thetaOffset = float(phiIndex % 16) * (sampleDelta * 0.0625);

    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    for (float theta = thetaOffset; theta <= HALF_PI; theta += sampleDelta) {
      float sinTheta = sin(theta);
      float cosTheta = cos(theta);

      // tangent space to world
      float3 sampleVec = right * (sinTheta * cosPhi) +
                         up * (sinTheta * sinPhi) + normal * cosTheta;

      sampleVec = normalize(sampleVec);

      float2 uv = Oct3To2(sampleVec) * 0.5 + 0.5;

      irradiance += RadianceInput.SampleLevel(float3(uv, Slice), 4.0).rgb *
                    cosTheta * sinTheta;

      nrSamples++;
    }

    phiIndex++;
  }

  return PI * irradiance / nrSamples;
}

#define BASE 32

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(int3 id: SV_DispatchThreadID) {
  if (id.x >= BASE || id.y >= BASE)
    return; // Out of bounds

  float2 uv = (float2(id.xy) + 0.5) / float2(BASE);

  float3 normal = DecodeOctahedron(uv);

  float3 irradiance = calculateIrradiance(normal);

  IrradianceOutput.Store(int3(id.xy, Slice), float4(irradiance, 1.0));
}
