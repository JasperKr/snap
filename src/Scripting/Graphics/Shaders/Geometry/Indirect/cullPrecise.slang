import drawlists;
import utils;
import constants;
import camera;
import Geometry.transform;
import material;
import meshletPayload;

uniform StructuredBuffer<DrawItem>.Handle DrawItems;
uniform StructuredBuffer<uint>.Handle DrawItemCount;
uniform StructuredBuffer<Mesh>.Handle Meshes;

bool IsVisible(CullData cullData, uint transformIndex) {
  float4x4 worldMatrix = GetModelMatrix(transformIndex);
  float4 center = mul(float4(cullData.GetPosition(), 1), worldMatrix);
  float radius = cullData.GetRadius();

  for (int i = 0; i < 6; ++i) {
    if (dot(center, CameraData[0].Frustum[i]) < -radius) {
      return false;
    }
  }

  float4 normalCone = cullData.GetCone();
  if (normalCone.w == 1.0) {
    return true;
  }

  float3 axis = normalize(mul(float4(normalCone.xyz, 0), worldMatrix)).xyz;
  float3 view = normalize(CameraData[0].Position - center.xyz);

  if (dot(view, -axis) > normalCone.w) {
    return false;
  }

  return true;
}

uint SearchInvocation(uint invocationID, out bool success) {
  bool found = false;

  int low = 0;
  int high = DrawItemCount[0];

  uint target = invocationID * 64; // Meshlet ID -> Vertex ID

  while (low <= high) {
    int mid = ((high - low) / 2U) + low;
    DrawItem currentItem = DrawItems[mid];

    // Prefix offset is stored as [count, prev count + count, prev count + count], so the lower bound is offset - count.
    uint currentItemStart = currentItem.PrefixOffset - currentItem.MeshletCount;
    success = target >= currentItemStart && target < currentItem.PrefixOffset;
    if (success) {
      return mid;
    }

    // The value we seek is lower than the current value
    if (currentItemStart > target) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  success = false;
  return uint32_t.maxValue;
}

groupshared TaskPayload output;

[shader("amplification")]
[outputtopology("triangle")]
[WaveSize(64)]
[numthreads(64, 1, 1)]
void amplificationMain(uint invocationID: SV_DispatchThreadID) {
  bool success = false;
  bool visible = false;

  uint invocation = SearchInvocation(invocationID, success);
  uint localMeshletIndex;

  if (success) {
    DrawItem drawItem = DrawItems[invocation];

    localMeshletIndex = invocationID - drawItem.ArrayOffset;
    CullData data = drawItem.MeshletCullData[localMeshletIndex];

    visible = IsVisible(data, drawItem.TransformIndex);
  }

  uint count = WaveActiveCountBits(visible);
  uint offset = WavePrefixCountBits(visible);

  if (visible) {
    output.MeshIndices[offset] = invocation;
    output.MeshletOffset[offset] = uint16_t(localMeshletIndex);
  }

  output.MeshletCount = count;
  DispatchMesh(count, 1, 1, output);
}
