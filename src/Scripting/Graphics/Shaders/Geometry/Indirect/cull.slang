import drawlists;
import utils;
import constants;
import camera;
import Geometry.transform;
import material;

static const uint DrawLayers = 3U;         // Opaque, Masked, Blend
static const uint PipelineStateCount = 3U; // Only varies in cull mode. Front, Back, None

uniform RWStructuredBuffer<DrawItem>.Handle DrawItems[9];
uniform RWStructuredBuffer<uint>.Handle MeshIndices[9];
uniform RWStructuredBuffer<uint>.Handle DrawItemCount;

uint GetBufferIndex(AlphaMode alphaMode, CullMode cullMode) { return alphaMode * 3 + cullMode; }

uniform StructuredBuffer<Mesh>.Handle Meshes;
uniform uint MeshCount;

uint SubgroupAtomicIndex(uint bufferIdx) {
  // uint count = WaveActiveSum(1U);
  uint idx = 0;
  // if (WaveIsFirstLane()) {
  // InterlockedAdd(DrawItemCount[bufferIdx], count, idx);
  // }

  // return WaveReadLaneFirst(idx) + WavePrefixSum(1U);

  InterlockedAdd(DrawItemCount[bufferIdx], 1, idx);

  return idx;
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
void computeMain(uint3 invocationID: SV_DispatchThreadID) {
  if (invocationID.x > MeshCount) {
    return;
  }

  Mesh mesh = Meshes[invocationID.x];
  float4x4 transform = mesh.GetTransform();

  float3 worldBmin = mesh.Lod.BoundsMin.xyz;
  float3 worldBmax = mesh.Lod.BoundsMax.xyz;
  TransformBounds(worldBmin, worldBmax, transform);

  if (!InsideFrustum(worldBmin, worldBmax)) {
    return;
  }

  int lodIndex = mesh.Lod.CalculateLODIndex(worldBmin, worldBmax);
  if (lodIndex < 0) { // Culled
    return;
  }

  MaterialData material = mesh.GetMaterial();
  uint bufferIdx = GetBufferIndex(material.alphaMode, material.cullMode);

  var DrawItemBuffer = DrawItems[bufferIdx];
  uint index = SubgroupAtomicIndex(bufferIdx);

  uint2 drawRange = mesh.Lod.DrawRanges[lodIndex];
  DrawItemBuffer[index] = DrawItem(mesh.Lod.Vertices, drawRange, mesh.MaterialIndex, mesh.TransformIndex);

  // TODO: Wait on thread idx - 1 to be done and add that to drawRange.y, to have an incrementing draw range
  // say we draw 4x 9 triangles, first one is 9, second 18, 27, 36.. , then we can binary search in the vtx shader
  // for the mesh info, (we could subgroup elect one to wait, then do the rest of the sum in subgroup ops)
  MeshIndices[bufferIdx][index] = drawRange.y;
}
