import drawlists;
import utils;
import constants;
import camera;
import transform;
import material;

static const uint DrawLayers = 3U;         // Opaque, Masked, Blend
static const uint PipelineStateCount = 3U; // Only varies in cull mode. Front, Back, None

struct VkDrawIndexedIndirectCommand {
  uint32_t indexCount;
  uint32_t instanceCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t firstInstance;
};

uniform RWStructuredBuffer<DrawItem>.Handle DrawItems[DrawLayers * PipelineStateCount];
uniform RWStructuredBuffer<VkDrawIndexedIndirectCommand>.Handle DrawInfo[DrawLayers * PipelineStateCount];
uniform RWStructuredBuffer<uint>.Handle DrawItemCount;

uint GetBufferIndex(AlphaMode alphaMode, CullMode cullMode) { return alphaMode * DrawLayers + cullMode; }

uniform StructuredBuffer<Mesh>.Handle Meshes;
uniform uint MeshCount;

uint SubgroupAtomicIndex(uint bufferIdx) {
  uint count = WaveActiveSum(1U);
  uint idx;
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
  DrawInfo[bufferIdx][index] = VkDrawIndexedIndirectCommand(drawRange.y - drawRange.x, 1, drawRange.x, 0, 0);
}
