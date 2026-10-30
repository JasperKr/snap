import raytracing;
import camera;

uniform float2 uv;

struct TraceResult {
  uint instanceIndex;
  uint primitiveIndex;
  uint hit;
  uint padding;
};

uniform RWStructuredBuffer<TraceResult>.Handle TraceResults;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  float3 rayOrigin = CameraData[0].Position;
  float3 rayDirection = CameraData[0].GetRayDirection(uv);

  RayDesc ray;
  ray.Origin = rayOrigin;
  ray.Direction = rayDirection;
  ray.TMin = CameraData[0].Near;
  ray.TMax = CameraData[0].Far;

  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE> q;
  q.TraceRayInline(GetSceneBVH(),
                   RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_FORCE_OPAQUE,
                   0xff, ray);

  // search for the closest hit
  while (q.Proceed()) {
  }

  if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
    TraceResults[0].hit = 1;
    TraceResults[0].instanceIndex = q.CommittedInstanceIndex();
    TraceResults[0].primitiveIndex = q.CommittedPrimitiveIndex();
  } else {
    TraceResults[0].hit = 0;
  }
}
