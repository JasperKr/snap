import raytracing;
import camera;
import utils;
import constants;
import random;

uniform Sampler2D.Handle DepthTexture;
uniform Sampler2D.Handle NormalTexture;
uniform WTexture2D<float>.Handle OutputTexture;

uniform uint FrameRandomState;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  uint width;
  uint height;
  OutputTexture.GetDimensions(width, height);

  if (id.x >= width || id.y >= height) {
    return;
  }

  float2 uv = (float2(id.xy) + 0.5) / float2(width, height);

  float3 normal = NormalTexture.SampleLevelZero(uv).xyz;
  normal = normalize(normal * 2.0f - 1.0f);
  float depth = DepthTexture.SampleLevelZero(uv).x;

  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    OutputTexture.Store(id.xy, 1);
    return;
  }

  float3 worldPosition = CameraData[0].GetWorldPosition(uv, depth);
  uint2 screenCoords = uint2(id.xy);
  uint randomState =
      FrameRandomState + screenCoords.x * 1702 + screenCoords.y * 40301;

  worldPosition += normal * 0.01f;

  int hitCount = 0;
  const int sampleCount = 1;

  for (int i = 0; i < sampleCount; ++i) {
    screenCoords += uint2(randomState, randomState * 84313U);
    float rnd0 = BlueNoiseTexture.Load(int3(screenCoords % 128, 0)).r;
    screenCoords += uint2(randomState * 3913U, randomState);
    float rnd1 = BlueNoiseTexture.Load(int3(screenCoords % 128, 0)).r;

    float3 direction = RandomHemisphereDirection(normal, rnd0, rnd1);
    RayDesc ray;
    ray.Origin = worldPosition;
    ray.Direction = direction;
    ray.TMin = 0.001f;
    ray.TMax = 1.0f;

    bool hit = TraceAnyHitRay(ray);

    if (hit) {
      hitCount++;
    }
  }

  OutputTexture.Store(id.xy, 1.0 - float(hitCount) / float(sampleCount));
}
