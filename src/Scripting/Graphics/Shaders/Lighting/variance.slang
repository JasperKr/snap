import camera;
import constants;
import utils;
import vrcs;

uniform Sampler2D.Handle AlbedoTexture;
uniform Sampler2D.Handle DepthTexture;
uniform Sampler2D<uint>.Handle NormalTexture;
uniform Sampler2D.Handle MaterialTexture;
uniform RWTexture2D<uint>.Handle VRCSModeTexture;

#define DEBUG_VRCS 1

#if DEBUG_VRCS
uniform WTexture2D.Handle VRCSModeDebugTexture;
#endif

static const float4[4] DebugColors = {
  { 0, 1, 0, 1 }, // Checker A: Green
  { 0, 0, 1, 1 }, // Checker B: Blue
  { 1, 0, 1, 1 }, // Quarter:   Purple
  { 1, 0, 0, 1 }, // Full-res:  Red
};

uniform float3 AlbedoWeights;
uniform float NormalWeight;
uniform float3 MaterialWeights;
uniform float DistanceWeights;
uniform float VarianceThreshold;

const static uint BlockSize = 2;
const static uint GroupSize = 8;

groupshared half3[GroupSize][GroupSize] albedos;
groupshared float [GroupSize][GroupSize] distances;
groupshared half3[GroupSize][GroupSize] normals;
groupshared half3[GroupSize][GroupSize] materials;
groupshared float [GroupSize][GroupSize] variances;

[shader("compute")]
[numthreads(GroupSize, GroupSize, 1)]
void computeMain(uint2 invocationID: SV_DispatchThreadID, uint2 groupLocalID: SV_GroupThreadID) {
  uint2 dimensions;
  AlbedoTexture.GetDimensions(dimensions.x, dimensions.y);

  if (any(invocationID > dimensions))
    return;

  float2 uv = (float2(invocationID.xy) + 0.5) / float2(dimensions);

  albedos[groupLocalID.x][groupLocalID.y] = half3(AlbedoTexture.Load(int3(invocationID, 0)).rgb);
  normals[groupLocalID.x][groupLocalID.y] = UnpackNormal(NormalTexture.Load(int3(invocationID, 0)));
  distances[groupLocalID.x][groupLocalID.y] = CameraData[0].GetDistance(uv, DepthTexture.Load(int3(invocationID, 0)).r);
  materials[groupLocalID.x][groupLocalID.y] = half3(MaterialTexture.Load(int3(invocationID, 0)).rgb);

  uint2 groupRoot = (groupLocalID / BlockSize) * BlockSize;

  GroupMemoryBarrierWithGroupSync();

  // Idk how to do this so random bullshit it is.

  // We have 4 modes:
  // CheckerA:
  // [X_]
  // [_X]
  // CheckerB:
  // [_X]
  // [X_]
  // Quarter:
  // [X_]
  // [__]
  // Full:
  // [XX]
  // [XX]

  // Logically we do quarter at the lowest variance between pixels
  // so we do that check for top left to all other corners, if variance < threshold use that
  // Then CheckerA/B, to pick A/B I'm thinking of picking
  // the two corners with the highest variance
  // And then full well, if both that highest variance is higher than some threshold

  // Quarter check
  if (all(groupLocalID == groupRoot)) // Only top-left thread checks.
  {
    half3 albedoSum = half3(0.0);
    half3 albedoSqrSum = half3(0.0);
    float distanceSum = 0.0;
    float distanceSqrSum = 0.0;
    half3 normalSum = half3(0.0);
    half3 normalSqrSum = half3(0.0);
    half3 materialSum = half3(0.0);
    half3 materialSqrSum = half3(0.0);

    for (uint x = 0; x < BlockSize; x++) {
      for (uint y = 0; y < BlockSize; y++) {
        uint2 samplePos = groupRoot + uint2(x, y);

        half3 albedo = albedos[samplePos.x][samplePos.y];
        albedoSum += albedo;
        albedoSqrSum += albedo * albedo;

        half3 normal = normals[samplePos.x][samplePos.y];
        normalSum += normal;
        normalSqrSum += normal * normal;

        half3 material = materials[samplePos.x][samplePos.y];
        materialSum += material;
        materialSqrSum += material * material;

        float distance = distances[samplePos.x][samplePos.y];
        distanceSum += distance;
        distanceSqrSum += distance * distance;
      }
    }

    // clang-format off
    albedoSum *= half(0.25); albedoSqrSum *= half(0.25);
    distanceSum *= 0.25; distanceSqrSum *= 0.25;
    normalSum *= half(0.25); normalSqrSum *= half(0.25);
    materialSum *= half(0.25); materialSqrSum *= half(0.25);
    // clang-format on

    half3 albedoVariance = sqrt(max(half3(0.0), albedoSqrSum - albedoSum * albedoSum));
    half3 normalVariance = sqrt(max(half3(0.0), normalSqrSum - normalSum * normalSum));
    half3 materialVariance = sqrt(max(half3(0.0), materialSqrSum - materialSum * materialSum));
    float distanceVariance = sqrt(max(0.0, distanceSqrSum - distanceSum * distanceSum));

    float variance = abs(dot(albedoVariance, AlbedoWeights)) + abs(dot(normalVariance, NormalWeight)) + abs(dot(materialVariance, MaterialWeights)) +
                     abs(distanceVariance * DistanceWeights);
    variance = max(0.0, variance);

    if (variance < VarianceThreshold) {
      VRCSModeTexture.Store(invocationID / BlockSize, VRCSMode.Quarter);
    }

    variances[groupRoot.x][groupRoot.y] = variance;
  }

  GroupMemoryBarrierWithGroupSync();

  // If last variance calculated by 2x2 is less than or equal to the threshold, we must've picked Quarter rate and we can early exit
  float variance = variances[groupRoot.x][groupRoot.y];
  if (variance < VarianceThreshold) {
#if DEBUG_VRCS
    VRCSModeDebugTexture.Store(invocationID, DebugColors[VRCSMode.Quarter]);
#endif

    return;
  }

  // CheckerA
  if (all(groupLocalID == groupRoot)) {
    half3 albedoSum = half3(0.0);
    half3 albedoSqrSum = half3(0.0);
    float distanceSum = 0.0;
    float distanceSqrSum = 0.0;
    half3 normalSum = half3(0.0);
    half3 normalSqrSum = half3(0.0);
    half3 materialSum = half3(0.0);
    half3 materialSqrSum = half3(0.0);

    for (uint i = 0; i < BlockSize; i++) {
      uint2 samplePos = groupRoot + i;

      half3 albedo = albedos[samplePos.x][samplePos.y];
      albedoSum += albedo;
      albedoSqrSum += albedo * albedo;

      half3 normal = normals[samplePos.x][samplePos.y];
      normalSum += normal;
      normalSqrSum += normal * normal;

      half3 material = materials[samplePos.x][samplePos.y];
      materialSum += material;
      materialSqrSum += material * material;

      float distance = distances[samplePos.x][samplePos.y];
      distanceSum += distance;
      distanceSqrSum += distance * distance;
    }

    // clang-format off
    albedoSum *= half(0.5); albedoSqrSum *= half(0.5);
    distanceSum *= 0.5; distanceSqrSum *= 0.5;
    normalSum *= half3(0.5); normalSqrSum *= half3(0.5);
    materialSum *= half(0.5); materialSqrSum *= half(0.5);
    // clang-format on

    half3 albedoVariance = sqrt(max(half3(0.0), albedoSqrSum - albedoSum * albedoSum));
    half3 normalVariance = sqrt(max(half3(0.0), normalSqrSum - normalSum * normalSum));
    half3 materialVariance = sqrt(max(half3(0.0), materialSqrSum - materialSum * materialSum));
    float distanceVariance = sqrt(max(0.0, distanceSqrSum - distanceSum * distanceSum));

    float variance = abs(dot(albedoVariance, AlbedoWeights)) + abs(dot(normalVariance, NormalWeight)) + abs(dot(materialVariance, MaterialWeights)) +
                     abs(distanceVariance * DistanceWeights);
    variance = max(0.0, variance);

    variances[groupRoot.x][groupRoot.y] = variance;
  }

  // CheckerB
  if (all(groupLocalID == (groupRoot + uint2(1, 0)))) {
    half3 albedoSum = half3(0.0);
    half3 albedoSqrSum = half3(0.0);
    float distanceSum = 0.0;
    float distanceSqrSum = 0.0;
    half3 normalSum = half3(0.0);
    half3 normalSqrSum = half3(0.0);
    half3 materialSum = half3(0.0);
    half3 materialSqrSum = half3(0.0);

    for (uint i = 0; i < BlockSize; i++) {
      uint2 samplePos = groupRoot + uint2(-i + 1, i);

      half3 albedo = albedos[samplePos.x][samplePos.y];
      albedoSum += albedo;
      albedoSqrSum += albedo * albedo;

      half3 normal = normals[samplePos.x][samplePos.y];
      normalSum += normal;
      normalSqrSum += normal * normal;

      half3 material = materials[samplePos.x][samplePos.y];
      materialSum += material;
      materialSqrSum += material * material;

      float distance = distances[samplePos.x][samplePos.y];
      distanceSum += distance;
      distanceSqrSum += distance * distance;
    }

    // clang-format off
    albedoSum *= half(0.5); albedoSqrSum *= half(0.5);
    distanceSum *= 0.5; distanceSqrSum *= 0.5;
    normalSum *= half3(0.5); normalSqrSum *= half3(0.5);
    materialSum *= half(0.5); materialSqrSum *= half(0.5);
    // clang-format on

    half3 albedoVariance = sqrt(max(half3(0.0), albedoSqrSum - albedoSum * albedoSum));
    half3 normalVariance = sqrt(max(half3(0.0), normalSqrSum - normalSum * normalSum));
    half3 materialVariance = sqrt(max(half3(0.0), materialSqrSum - materialSum * materialSum));
    float distanceVariance = sqrt(max(0.0, distanceSqrSum - distanceSum * distanceSum));

    float variance = abs(dot(albedoVariance, AlbedoWeights)) + abs(dot(normalVariance, NormalWeight)) + abs(dot(materialVariance, MaterialWeights)) +
                     abs(distanceVariance * DistanceWeights);
    variance = max(0.0, variance);

    variances[groupRoot.x + 1][groupRoot.y] = variance;
  }

  GroupMemoryBarrierWithGroupSync();

  if (all(groupLocalID == groupRoot)) {
    float varCheckerA = variances[groupRoot.x][groupRoot.y];
    float varCheckerB = variances[groupRoot.x + 1][groupRoot.y];

    if (min(varCheckerA, varCheckerB) > VarianceThreshold * 2.0) {
      VRCSModeTexture.Store(invocationID / BlockSize, VRCSMode.Full);
    } else if (varCheckerA >= varCheckerB) {
      VRCSModeTexture.Store(invocationID / BlockSize, VRCSMode.CheckerA);
    } else {
      VRCSModeTexture.Store(invocationID / BlockSize, VRCSMode.CheckerB);
    }
  }

#if DEBUG_VRCS
  GroupMemoryBarrierWithGroupSync();

  VRCSMode mode = (VRCSMode)VRCSModeTexture.Load(invocationID / BlockSize);
  float4 debugColor = DebugColors[mode];
  VRCSModeDebugTexture.Store(invocationID, debugColor);
#endif
}
