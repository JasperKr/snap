import raytracing;
import camera;
import utils;
import constants;
import random;

uniform Sampler2D<float>.Handle DepthTexture;
uniform Sampler2D<float>.Handle PreviousDepthTexture;

uniform Sampler2D<float2>.Handle MotionTexture;

uniform Sampler2D<float>.Handle RayHitTexture;
uniform WTexture2D<float2>.Handle OutputTexture;
uniform Sampler2D<float2>.Handle PreviousOutputTexture;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint2 id: SV_DispatchThreadID) {
  uint width;
  uint height;
  OutputTexture.GetDimensions(width, height);

  if (id.x >= width || id.y >= height) {
    return;
  }

  float2 texelSize = rcp(float2(width, height));

  float2 uv = (float2(id) + 0.5) / float2(width, height);

  float depth = DepthTexture.SampleLevelZero(uv);

  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    OutputTexture.Store(id, float2(1.0, 0.0));
    return;
  }

  float3 worldPosition = CameraData[0].GetWorldPosition(uv, depth);

  float2 sampleCoord = float2(uv);
  sampleCoord.xy -= UnpackMotionVector(MotionTexture.Load(int3(id, 0)));

  float visibility = RayHitTexture.Load(int3(id, 0));

  if (any(sampleCoord.xy < texelSize * 0.5) ||
      any(sampleCoord.xy >= 1.0 - texelSize * 0.5)) {
    OutputTexture.Store(id, float2(visibility, 0.0));
    return;
  }

  float previousDepth = PreviousDepthTexture.SampleLevelZero(sampleCoord);
  float3 previousWorldPosition =
      CameraData[1].GetWorldPosition(sampleCoord, previousDepth);

  float rejection = 0.0F;

  // Increase rejection of previous sample based on distance between current and previous world position
  rejection += length(worldPosition - previousWorldPosition);

  float2 pixelSize = float2(1.0F / width, 1.0F / height);
  float sum = 0.0;
  float sum2 = 0.0;
  uint count = 0;

  [unroll]
  for (int x = -1; x <= 1; ++x) {
    [unroll]
    for (int y = -1; y <= 1; ++y) {
      float2 neighbourCoord = sampleCoord + float2(x, y) * pixelSize;
      if (any(neighbourCoord < pixelSize * 0.5) ||
          any(neighbourCoord >= 1.0 - pixelSize * 0.5)) {
        continue;
      }

      count++;
      float value = PreviousOutputTexture.SampleLevelZero(neighbourCoord).x;
      sum += value;
      sum2 += value * value;
    }
  }

  float mean = sum / count;
  float mean2 = sum2 / count;
  float variance = max(0.0, mean2 - mean * mean);
  float sigma = sqrt(variance);

  const float gamma = 1.0; // tune between ~0.5 and 2.0

  float2 currentAO = PreviousOutputTexture.SampleLevelZero(sampleCoord);
  float influence = rejection;
  influence += 1.0F / (32.0F * currentAO.y);
  influence = clamp(influence, 0.0F, 1.0F);

  currentAO.x = clamp(currentAO.x, mean - gamma * sigma, mean + gamma * sigma);

  float newAO = lerp(currentAO.x, visibility, influence);

  OutputTexture.Store(id, float2(newAO, currentAO.y + 0.05F - rejection));
}
