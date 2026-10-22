
import utils;
import constants;
import camera;

Sampler2D<float> DepthTexture;
Sampler2D<float> PreviousDepthTexture;
Sampler2D<float2> MotionTexture;
Sampler2D<uint> NormalTexture;

#include "fullscreen.slang"

[shader("fragment")]
float fragmentMain(float2 texCoords: TEXCOORDS) {
  float depth = DepthTexture.SampleLevelZero(texCoords);
  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    return 1.0;
  }

  float2 motion = UnpackMotionVector(MotionTexture.SampleLevelZero(texCoords));
  float2 previousTexCoords = texCoords - motion;

  float previousDepth = PreviousDepthTexture.SampleLevelZero(previousTexCoords);

  if (previousDepth == DepthFarPlane || previousDepth == DepthNearPlane) {
    return 1.0;
  }

  float3 worldPosition = CameraData[0].GetWorldPosition(texCoords, depth);
  float3 previousWorldPosition =
      CameraData[0].GetWorldPosition(previousTexCoords, previousDepth);

  float distance = length(worldPosition - previousWorldPosition);

  float variance = 0.0f;

  variance += distance * 4.0F; // 25cm -> Full rejection

  if (any(previousTexCoords < 0.0F) || any(previousTexCoords > 1.0F)) {
    variance += 1.0F;
  }

  float2 texSize;
  DepthTexture.GetDimensions(texSize.x, texSize.y);
  float2 texelSize = 1.0F / texSize;

  float3 normal = UnpackNormal(NormalTexture.SampleLevelZero(texCoords));

  float neighbourhoodVariance = 0.0F;
  float weightSum = 0.0F;

  [unroll]
  for (int x = -1; x <= 1; ++x) {
    [unroll]
    for (int y = -1; y <= 1; ++y) {
      float2 sampleCoords = texCoords + float2(x, y) * texelSize;
      if (any(sampleCoords < texelSize * 0.5) ||
          any(sampleCoords >= 1.0 - texelSize * 0.5)) {
        continue;
      }

      if (x == 0 && y == 0) {
        continue;
      }

      float depth = DepthTexture.SampleLevelZero(sampleCoords);
      if (depth == DepthFarPlane || depth == DepthNearPlane) {
        continue;
      }

      float3 sampleWorldPosition =
          CameraData[0].GetWorldPosition(sampleCoords, depth);
      float3 sampleNormal =
          UnpackNormal(NormalTexture.SampleLevelZero(sampleCoords));

      float3 worldToSample = sampleWorldPosition - worldPosition;

      float depthError = abs(dot(sampleNormal, worldToSample));
      float normalWeight = saturate(dot(sampleNormal, normal));
      normalWeight *= normalWeight;

      float weight = exp(-depthError * 50.0F) * normalWeight;

      neighbourhoodVariance += weight * depthError * depthError;
      weightSum += weight;
    }
  }

  if (weightSum > 0.0F) {
    neighbourhoodVariance /= weightSum;
  }

  variance += neighbourhoodVariance * 0.5F; // 50cm -> Full rejection

  return saturate(variance) * 2.0 - 1.0;
}
