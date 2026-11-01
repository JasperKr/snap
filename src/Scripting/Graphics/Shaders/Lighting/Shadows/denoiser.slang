import raytracing;
import camera;
import utils;
import constants;
import random;

uniform Sampler2D.Handle DepthTexture;
uniform Sampler2D.Handle PreviousDepthTexture;
uniform Sampler2D.Handle MotionTexture;

uniform Sampler2D<uint>.Handle RayHitTexture;
uniform WTexture2DArray<float>.Handle OutputTexture;
uniform Sampler2DArray<float>.Handle PreviousOutputTexture;

uniform uint LayerOffset;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  uint width;
  uint height;
  uint layerCount;
  OutputTexture.GetDimensions(width, height, layerCount);

  if (id.x >= width || id.y >= height) {
    return;
  }

  id.z += LayerOffset;

  float2 uv = (float2(id.xy) + 0.5) / float2(width, height);

  float depth = DepthTexture.SampleLevelZero(uv).x;

  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    OutputTexture.Store(id, 1.0);
    return;
  }

  float3 worldPosition = CameraData[0].GetWorldPosition(uv, depth);

  float3 sampleCoord = float3(uv, id.z);
  sampleCoord.xy -= UnpackMotionVector(MotionTexture.Load(int3(id.xy, 0)).xy);

  bool inShadow = (RayHitTexture.Load(int3(id.xy, 0)).x & (1 << id.z)) != 0;

  if (any(sampleCoord < 0.0) || any(sampleCoord.xy >= 1.0)) {
    OutputTexture.Store(id, inShadow ? 0.0 : 1.0);
    return;
  }

  float previousDepth = PreviousDepthTexture.SampleLevelZero(sampleCoord.xy).x;
  float3 previousWorldPosition =
      CameraData[1].GetWorldPosition(sampleCoord.xy, previousDepth);

  float influence = 1.0F / 20.0F;

  // Increase influence of current sample based on distance between current and previous world position
  influence += length(worldPosition - previousWorldPosition);
  influence = clamp(influence, 0.0F, 1.0F);

  float2 pixelSize = float2(1.0F / width, 1.0F / height);
  float sum = 0.0;
  uint count = 0;

  [unroll]
  for (int x = -2; x <= 2; ++x) {
    [unroll]
    for (int y = -2; y <= 2; ++y) {
      float2 neighbourCoord = uv + float2(x, y) * pixelSize;
      if (any(neighbourCoord < 0.0) || any(neighbourCoord >= 1.0)) {
        continue;
      }

      count++;

      float neighbourShadow =
          1.0 - float(RayHitTexture.SampleLevelZero(neighbourCoord) &
                      (1 << id.z) != 0);

      sum += neighbourShadow;
    }
  }

  float mean = sum / count;
  float variance = mean * (1.0 - mean);
  float sigma = sqrt(variance);
  const float gamma = 1.5;

  float currentShadow = PreviousOutputTexture.SampleLevelZero(sampleCoord);

  // If the new shadow value is outside the neighbourhood range, clamp it to the range
  currentShadow =
      clamp(currentShadow, mean - gamma * sigma, mean + gamma * sigma);

  float newShadow = lerp(currentShadow, inShadow ? 0.0 : 1.0, influence);

  // If the stored shadow value is 1 / 255 or something and the new shadow value is 0 / 255
  // It might keep rounding up to 1, causing ghost shadows. This forces the shadow value to favour the new shadow value
  // If it is stuck.
  uint currentRoundedShadow = uint(round(newShadow * 255.0F));
  uint previousRoundedShadow = uint(round(currentShadow * 255.0F));

  const float kQuantizationStep = 1.0 / 255.0;

  if (currentRoundedShadow == previousRoundedShadow) {
    float bias = inShadow ? -kQuantizationStep : kQuantizationStep;
    newShadow = saturate(newShadow + bias);
  }

  OutputTexture.Store(id, newShadow);
}
