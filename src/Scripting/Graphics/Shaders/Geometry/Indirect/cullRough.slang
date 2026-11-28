import drawlists;
import utils;
import constants;
import camera;
import material;
import Geometry.transform;

uniform RWStructuredBuffer<DrawItem>.Handle DrawItems[9];
uniform RWStructuredBuffer<uint>.Handle DrawItemCount;
uniform StructuredBuffer<Mesh>.Handle Meshes;

uint GetBufferIndex(AlphaMode alphaMode, CullMode cullMode) { return alphaMode * 3 + cullMode; }

uint SubgroupAtomicIndex(uint bufferIdx) {
  uint count = WaveActiveSum(1U);
  uint idx = 0;
  if (WaveIsFirstLane()) {
    InterlockedAdd(DrawItemCount[bufferIdx], count, idx);
  }

  return WaveReadLaneFirst(idx) + WavePrefixSum(1U);
}

bool InsideFrustum(float3 bmin, float3 bmax) {
  for (int i = 0; i < 6; i++) {
    int out = 0;
    float4 plane = CameraData[0].Frustum[i];

    out += dot(plane.xyz, float3(bmin.x, bmin.y, bmin.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmax.x, bmin.y, bmin.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmin.x, bmax.y, bmin.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmax.x, bmax.y, bmin.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmin.x, bmin.y, bmax.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmax.x, bmin.y, bmax.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmin.x, bmax.y, bmax.z)) + plane.w < 0.0F ? 1 : 0;
    out += dot(plane.xyz, float3(bmax.x, bmax.y, bmax.z)) + plane.w < 0.0F ? 1 : 0;

    if (out == 8) {
      return false;
    }
  }

  return true;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint invocation: SV_DispatchThreadID) {
  Mesh mesh = Meshes[invocation];

  float4x4 transform = mesh.GetTransform();
  float3 worldBmin = mesh.Lod.BoundsMin.xyz;
  float3 worldBmax = mesh.Lod.BoundsMax.xyz;

  TransformBounds(worldBmin, worldBmax, transform);

  int lodIndex = mesh.Lod.CalculateLODIndex(worldBmin, worldBmax);
  if (lodIndex < 0) { // Culled
    return;
  }

  MaterialData material = mesh.GetMaterial();
  uint2 drawrange = mesh.Lod.LodMeshletRanges[lodIndex];

  if (drawrange.y == 0) {
    return;
  }

  uint bufferIdx = GetBufferIndex(material.alphaMode, material.cullMode);
  uint index = SubgroupAtomicIndex(bufferIdx);

  var drawItemsForMaterial = DrawItems[bufferIdx];

  drawItemsForMaterial[index] =
      DrawItem(mesh.Lod.Vertices, mesh.Lod.Meshlets, mesh.Lod.MeshletCullData, drawrange.x, drawrange.y, 0, mesh.MaterialIndex, mesh.TransformIndex);

  if (index == 0) {
    drawItemsForMaterial[index].PrefixOffset = drawrange.y;
    return;
  }

  GroupMemoryBarrierWithWaveSync();

  uint prefixOffset = drawrange.y;
  uint previousIndex = index - 1;

  while (true) {
    DrawItem item = drawItemsForMaterial[previousIndex];

    while (item.MeshletCount == 0) {
      DeviceMemoryBarrier();
      item = drawItemsForMaterial[previousIndex];
    }

    if (previousIndex == 0) {
      while (item.PrefixOffset == 0) {
        DeviceMemoryBarrier();
        item = drawItemsForMaterial[previousIndex];
      }
    }

    // If the item has a full prefix sum, we're done
    if (item.PrefixOffset != 0) {
      prefixOffset += item.PrefixOffset;
      break;
    } else if (item.MeshletCount != 0) {
      prefixOffset += item.MeshletCount;
    }

    // Wait for new memory to be available, previous item did not have a count nor a global sum available
    DeviceMemoryBarrier();

    previousIndex--;
  }

  drawItemsForMaterial[index].PrefixOffset = prefixOffset;
}
