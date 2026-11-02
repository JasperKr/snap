module acceleration;

// I guess acceleration is a weird name for it,
// But acceleration structure or whatever is too long
// bvh is a bit short and idk

import volume;

public uniform uint2 VolumeBVH;
public uniform StructuredBuffer<Volume>.Handle Volumes;

public RaytracingAccelerationStructure GetVolumeBVH() {
  uint64_t address = VolumeBVH.x;
  address |= (uint64_t)(VolumeBVH.y) << 32;

  return (RaytracingAccelerationStructure)address;
}
