import volume;
import acceleration;

struct VolumeRef {
  half tMin;
  uint16_t index;
};

const static uint StackSize = 8;

inline void Swap(inout VolumeRef first, inout VolumeRef second) {
  if (first.tMin > second.tMin) {
    VolumeRef temp = first;
    second = first;
    first = temp;
  }
}

uint64_t GetIntersections(RayDesc ray) {
  VolumeRef[StackSize] stack;
  uint stackIdx = 0;

  for (int i = 0; i < StackSize; i++) {
    stack[i].tMin = half.maxValue;
  }

  // Trace all hits up to 8 intersections
  RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
           RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;

  q.TraceRayInline(GetVolumeBVH(),
                   RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
                       RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
                   0xff, ray);
  bool trace = true;

  while (trace && stackIdx < StackSize) {
    while (q.Proceed()) {
    }

    trace = q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

    if (trace) {
      stack[stackIdx++] = VolumeRef((half)q.CommittedRayT(),
                                    (uint16_t)q.CommittedInstanceIndex() + 1);
    }
  }

  // Bitonic sort of 8 lanes.

  // H==H  H==H  H==H  H==H
  // H==|==|==H  H==|==|==H
  // |  H==H  |  |  H==H  |
  // H==H  H==H  H==H  H==H
  // H==|==|==|==|==|==|==H
  // |  H==|==|==|==|==H  |
  // |  |  H==|==|==H  |  |
  // |  |  |  H==H  |  |  |
  // H==|==H  |  H==|==H  |
  // |  H==|==H  |  H==|==H
  // H==H  H==H  H==H  H==H

  // clang-format off
  Swap(stack[0], stack[1]); Swap(stack[2], stack[3]); Swap(stack[4], stack[5]); Swap(stack[6], stack[7]);
  Swap(stack[0], stack[3]); Swap(stack[4], stack[7]);
  Swap(stack[1], stack[2]); Swap(stack[5], stack[6]);
  Swap(stack[0], stack[1]); Swap(stack[2], stack[3]); Swap(stack[4], stack[5]); Swap(stack[6], stack[7]);
  Swap(stack[0], stack[7]);
  Swap(stack[1], stack[6]);
  Swap(stack[2], stack[5]);
  Swap(stack[3], stack[4]);
  Swap(stack[1], stack[3]); Swap(stack[5], stack[7]);
  Swap(stack[0], stack[2]); Swap(stack[4], stack[6]);
  Swap(stack[0], stack[1]); Swap(stack[2], stack[3]); Swap(stack[4], stack[5]); Swap(stack[6], stack[7]);
  // clang-format on

  uint64_t volumes = uint64_t(0ULL);
  for (int i = 0; i < StackSize; i++) {
    volumes |= stack[i].index << i * 8UL;
  }

  return volumes;
}

const static uint SampleCount = 16;

float DetermineStepSize(RayDesc ray, uint64_t volumes) {
  float startT = 0.0F;
  float currentEndT = 0.0F;
  float distance = 0.0F;

  // Say we have 3 boxes.
  // [        ]   [     [________]_____]
  // t0-a  t1-a   t0-b  t0-c  t1-b  t1-c

  // Iter 0
  // A:
  // t0 > 0 and currentEndT == 0 thus we enter the else
  // distance += 0.0 - 0.0;
  // startT = N0
  // currentEndT = N1

  // Iter 1
  // B:
  // t0 > 0 and t0 > currentEndT thus we enter the else
  // distance += N1 - N0;
  // startT = N2;
  // currentEndT = N3;

  // Iter 2
  // C:
  // t0 > 0 and t0 < currendEndT thus we enter the if
  // currentEndT = N5

  // Done
  // distance += N5 - N2;

  for (int i = 0; i < StackSize; i++) {
    uint idx = uint(volumes >> i * 8UL) & 0xFF;

    if (idx == 0) {
      break;
    }

    var volume = Volumes[idx - 1];
    float t0;
    float t1;

    if (!volume.TraceRay(ray, t0, t1)) {
      break;
    }

    if (t0 < currentEndT) {
      currentEndT = t1;
    } else {
      distance += currentEndT - startT;
      startT = t0;
      currentEndT = t1;
    }
  }

  distance += currentEndT - startT;

  return distance / SampleCount;
}

float4 TraceRay(RayDesc ray) {
  uint64_t hits = GetIntersections(ray);

  float t = 0.0F;
  float stepSize = DetermineStepSize(ray, hits);

  uint8_t currentVolumes;
  uint8_t idx;
  uint8_t availableVolumes = 0xFF;

  // Incoming Light + Transmittance as w
  float4 incomingLightTransmittance;

  for (int i = 0; i < SampleCount; i++) {
    // TODO: Get all volumes at t0, add to stack
    // can probably use VolumeRef's again, or one with more info for tMax
    // if out of range of any volume, pop from stack
    // if no volumes, go to next volume t0 and continue from there

    // If we're out of volumes to sample, add a new one from the stack.
    if (currentVolumes == 0UL) {
      availableVolumes &= ~(uint8_t(1U) << idx);
      currentVolumes |= uint8_t(1U) << idx;
      uint8_t volume = uint8_t(hits >> idx++ * 8UL) & 0xFF;

      // oob or no volumes left in hit stack
      if (idx == StackSize || volume == 0) {
        break;
      }

      float t1;
      // Update t
      Volumes[volume].TraceRay(ray, t, t1);
    }

    // Add all volumes we can start sampling now
    bool added = true;

    // We might add multiple at a time
    while (added) {
      added = false;

      // Find the bit idx of the lowest volume not yet added
      int bitIdx = firstbitlow(availableVolumes);

      // Get it's index
      uint8_t volume = uint8_t(hits >> bitIdx * 8UL) & 0xFF;
      if (volume != 0) {
        float t0;
        float t1;

        // Get t0, t1
        Volumes[volume].TraceRay(ray, t0, t1);

        // If the current raymarch time is within that time, we need to sample it.
        if (t >= t0 && t <= t1) {
          currentVolumes |= uint8_t(1U) << bitIdx;
          availableVolumes &= ~(uint8_t(1U) << bitIdx);

          added = true;
        }
      }
    }

    float density = 0.0F;
    float anisotropy = 0.0F;
    float3 samplePosition = ray.Origin + ray.Direction * t;

    uint8_t iterVolumes = currentVolumes;
    while (iterVolumes != 0) {
      int bitIdx = firstbitlow(iterVolumes);
      iterVolumes &= ~(uint8_t(1UL) << bitIdx);

      // Load index from Idx in stack
      uint idx = uint8_t(hits >> bitIdx * 8UL) & 0xFF;
      var volume = Volumes[idx];

      if (!volume.ContainsPoint(samplePosition)) {

        // Stop iterating this volume when we exit it's range.
        currentVolumes &= ~(uint8_t(1UL) << bitIdx);
        continue;
      }

      density += volume.SampleDensity(samplePosition);
      anisotropy += volume.anisotropy;
    }

    // TODO: In scattering with incomingLightTransmittance
    // Loop over lights and accumulate stuff ig, implement clustered lighting?
  }
}
