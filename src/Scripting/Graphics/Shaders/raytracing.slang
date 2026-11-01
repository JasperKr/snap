module raytracing;

public uniform uint2 SceneBVH;

public RaytracingAccelerationStructure GetSceneBVH() {
  uint64_t address = SceneBVH.x;
  address |= (uint64_t)(SceneBVH.y) << 32;

  return (RaytracingAccelerationStructure)address;
}

public bool TraceAnyHitRay(RayDesc ray) {
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
           RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
           RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

  q.TraceRayInline(GetSceneBVH(),
                   RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
                       RAY_FLAG_CULL_BACK_FACING_TRIANGLES |
                       RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                   0xff, ray);

  while (q.Proceed()) {
  }

  return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}
