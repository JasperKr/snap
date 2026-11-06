import volume;
import acceleration;
import constants;
import camera;
import random;

import directionalLight;

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

uint64_t GetIntersections(RayDesc ray, out uint hitCount) {
  VolumeRef[StackSize] stack;
  hitCount = 0;

  for (int i = 0; i < StackSize; i++) {
    stack[i].tMin = half.maxValue;
    stack[i].index = 0;
  }

  // Trace all hits up to 8 intersections
  RayQuery<0> q;

  q.TraceRayInline(GetVolumeBVH(), 0, 0xff, ray);
  bool trace = true;

  while (q.Proceed()) {
    stack[hitCount] = VolumeRef((half)q.CandidateTriangleRayT(), (uint16_t)q.CandidateInstanceIndex() + uint16_t(1));
    hitCount++;

    if (hitCount == StackSize)
      break;
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
  for (uint16_t i = 0; i < hitCount; i++) {
    volumes |= uint64_t(stack[i].index) << uint64_t(i * 8UL);
  }

  return volumes;
}

const static uint SampleCount = 16;
const static uint TransmittanceOnlySampleCount = 8;

float DetermineStepSize(RayDesc ray, uint64_t volumes, uint sampleCount) {
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
    uint idx = uint(volumes >> (i * 8UL)) & 0xFF;

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
      distance += max(0.0, currentEndT - startT);
      startT = t0;
      currentEndT = t1;
    }

    if (currentEndT >= ray.TMax) {
      break;
    }
  }

  distance += min(currentEndT, ray.TMax) - startT;

  return distance / sampleCount;
}

float TraceTransmittanceRay(RayDesc ray) {
  uint hitCount = 0;
  uint64_t hits = GetIntersections(ray, hitCount);

  float t = ray.TMin;
  float stepSize = DetermineStepSize(ray, hits, TransmittanceOnlySampleCount);

  uint8_t currentVolumes = 0;
  uint8_t idx = 0;
  uint8_t availableVolumes = 0xFF;

  float density = 0.0F;

  for (int i = 0; i < SampleCount; i++) {
    // TODO: Get all volumes at t0, add to stack
    // can probably use VolumeRef's again, or one with more info for tMax
    // if out of range of any volume, pop from stack
    // if no volumes, go to next volume t0 and continue from there

    // If we're out of volumes to sample, add a new one from the stack.
    if (currentVolumes == 0UL) {
      availableVolumes &= ~(uint8_t(1U) << idx);
      currentVolumes |= uint8_t(1U) << idx;
      uint8_t volume = uint8_t(hits >> (idx++ * uint8_t(8UL))) & uint8_t(0xFF);

      // oob or no volumes left in hit stack
      if (idx == StackSize || volume == 0) {
        break;
      }

      float t1;
      // Update t
      Volumes[volume - uint8_t(1)].TraceRay(ray, t, t1);
    }

    // Add all volumes we can start sampling now
    bool added = true;

    // We might add multiple at a time
    while (added) {
      added = false;

      // Find the bit idx of the lowest volume not yet added
      uint8_t bitIdx = uint8_t(firstbitlow(availableVolumes));

      // Get it's index
      uint8_t volume = uint8_t(hits >> uint64_t(bitIdx * uint8_t(8UL))) & uint8_t(0xFF);
      if (volume != 0) {
        float t0;
        float t1;

        // Get t0, t1
        Volumes[volume - uint8_t(1)].TraceRay(ray, t0, t1);

        // If the current raymarch time is within that time, we need to sample it.
        if (t >= t0 && t <= t1) {
          currentVolumes |= uint8_t(1U) << bitIdx;
          availableVolumes &= ~(uint8_t(1U) << bitIdx);

          added = true;
        }
      }
    }

    float3 samplePosition = ray.Origin + ray.Direction * t;

    uint8_t iterVolumes = currentVolumes;
    while (iterVolumes != 0) {
      uint8_t bitIdx = uint8_t(firstbitlow(iterVolumes));
      iterVolumes &= ~(uint8_t(1UL) << bitIdx);

      // Load index from Idx in stack
      uint idx = uint8_t(hits >> uint64_t(bitIdx * uint8_t(8UL))) & uint8_t(0xFF);
      var volume = Volumes[idx - 1];

      if (!volume.ContainsPoint(samplePosition)) {

        // Stop iterating this volume when we exit it's range.
        iterVolumes &= ~(uint8_t(1UL) << bitIdx);
        continue;
      }

      density += volume.SampleDensity(samplePosition);
    }
  }

  return exp(-density);
}

float3 SampleIncomingLight(float3 position, float3 viewDirection, float anisotropy) {
  float3 incomingLight = float3(0.0);

  for (int i = 0; i < DirectionalLightCount; i++) {
    var light = DirectionalLights[i];

    float3 lightDirection = light.GetForward();
    RayDesc ray = RayDesc(position, 0.0, -lightDirection, 100.0);

    float transmittance = TraceTransmittanceRay(ray);

    float cosTheta = dot(viewDirection, -lightDirection);
    float phase = Volume.PhaseFunction(cosTheta, anisotropy);

    incomingLight += transmittance * light.Color * light.Intensity * phase;
  }

  return incomingLight * (1.0 / (PI * 4.0));
}

uniform uint FrameRandomState;

float4 TraceRay(RayDesc ray, uint rndState) {
  uint hitCount = 0;
  uint64_t hits = GetIntersections(ray, hitCount);

  float t = ray.TMin;
  float stepSize = DetermineStepSize(ray, hits, SampleCount);

  uint8_t currentVolumes = 0;
  uint8_t idx = 0;
  uint8_t availableVolumes = 0x0;

  for (int i = 0; i < hitCount; i++) {
    availableVolumes |= uint8_t(1) << uint8_t(i);
  }

  // Incoming Light + Transmittance as w
  float4 incomingLightTransmittance = float4(0.0, 0.0, 0.0, 1.0);

  for (int i = 0; i < SampleCount; i++) {
    // If we're out of volumes to sample, add a new one from the stack.
    if (currentVolumes == 0UL) {
      if (availableVolumes == 0)
        break;

      availableVolumes &= availableVolumes - uint8_t(1);
      currentVolumes |= uint8_t(1U) << idx;
      uint8_t volume = uint8_t(hits >> (idx * uint8_t(8UL)));
      idx++;

      // oob or no volumes left in hit stack
      if (idx == StackSize || volume == 0) {
        break;
      }

      float t1;
      // Update t
      Volumes[volume - uint8_t(1)].TraceRay(ray, t, t1);
      t += 0.001;
      t += RandomValue(rndState) * stepSize;
    }

    // Add all volumes we can start sampling now
    bool added = true;

    // We might add multiple at a time
    while (added) {
      added = false;

      // Find the bit idx of the lowest volume not yet added
      uint8_t bitIdx = uint8_t(firstbitlow(availableVolumes));

      // Get it's index
      uint8_t volume = uint8_t(hits >> uint64_t(bitIdx * uint8_t(8UL)));
      if (volume != 0) {
        float t0;
        float t1;

        // Get t0, t1
        bool hit = Volumes[volume - uint8_t(1)].TraceRay(ray, t0, t1);

        // If the current raymarch time is within that time, we need to sample it.
        if (hit && t >= t0 && t <= t1) {
          currentVolumes |= uint8_t(1U) << bitIdx;
          availableVolumes &= availableVolumes - uint8_t(1U);

          added = true;
        }
      }
    }

    float density = 0.0F;
    float anisotropy = 0.0F;
    float3 samplePosition = ray.Origin + ray.Direction * t;

    uint8_t iterVolumes = currentVolumes;
    float3 fogColorSqr = float3(0.0);
    float fogBlendCnt = 0.0F;

    while (iterVolumes != 0) {
      uint8_t bitIdx = uint8_t(firstbitlow(iterVolumes));
      uint8_t removeMask = iterVolumes - uint8_t(1);
      iterVolumes &= removeMask;

      // Load index from Idx in stack
      uint idx = uint8_t(hits >> uint64_t(bitIdx * uint8_t(8UL)));
      var volume = Volumes[idx - 1];

      if (!volume.ContainsPoint(samplePosition)) {
        // Stop iterating this volume when we exit it's range.
        currentVolumes &= removeMask;

        continue;
      }

      density += volume.SampleDensity(samplePosition);
      anisotropy += volume.anisotropy;

      float3 color = volume.GetColor(samplePosition);
      fogColorSqr += color * color;
      fogBlendCnt++;
    }

    // float3 newIncomingLight =
    //     SampleIncomingLight(samplePosition, ray.Direction, anisotropy);
    float3 newIncomingLight = float3(0.5);
    float transmittance = exp(-density * stepSize);

    newIncomingLight *= sqrt(fogColorSqr / max(fogBlendCnt, 1.0));
    newIncomingLight -= newIncomingLight * transmittance;
    incomingLightTransmittance.xyz += newIncomingLight * incomingLightTransmittance.w;
    incomingLightTransmittance.w *= transmittance;

    t += stepSize;
  }

  return incomingLightTransmittance;
}

uniform WTexture2D<float4>.Handle OutputTexture;
uniform Sampler2D<float>.Handle DepthTexture;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint2 invocationID: SV_DispatchThreadID) {
  uint2 outputTextureSize;
  OutputTexture.GetDimensions(outputTextureSize.x, outputTextureSize.y);

  if (any(invocationID > outputTextureSize)) {
    return;
  }

  float2 uv = (float2(invocationID) + 0.5) / float2(outputTextureSize);

  RayDesc ray = RayDesc(CameraData[0].Position, CameraData[0].Near, CameraData[0].GetRayDirection(uv), CameraData[0].Far);

  float depth = DepthTexture.SampleLevelZero(uv);
  ray.TMax = min(ray.TMax, CameraData[0].GetDistance(uv, depth));
  ray.TMax = max(ray.TMax, ray.TMin);

  uint state = FrameRandomState;
  state += outputTextureSize.y * 15421 * invocationID.x + invocationID.y * 9310 * outputTextureSize.x;

  float4 incomingLightTransmittance = TraceRay(ray, state);

  OutputTexture.Store(invocationID, incomingLightTransmittance);
}
