module raytracing;

public uniform RaytracingAccelerationStructure.Handle SceneBVH;

public bool TraceAnyHitRay(RayDesc ray) {
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
           RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
           RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

  q.TraceRayInline(SceneBVH,
                   RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
                       RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
                       RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                   0xff, ray);

  while (q.Proceed()) {
  }

  return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}
