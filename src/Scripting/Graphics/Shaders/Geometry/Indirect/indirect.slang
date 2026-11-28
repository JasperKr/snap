static const uint DrawLayers = 3U;         // Opaque, Masked, Blend
static const uint PipelineStateCount = 3U; // Only varies in cull mode. Front, Back, None

struct VkDrawMeshTasksIndirectCommandEXT {
  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;
};

uniform RWStructuredBuffer<VkDrawMeshTasksIndirectCommandEXT>.Handle DrawInfo;
uniform StructuredBuffer<uint>.Handle DrawItemCount;

[shader("compute")]
[numthreads(DrawLayers * PipelineStateCount, 1, 1)]
void computeMain(uint invocationID: SV_DispatchThreadID) { DrawInfo[invocationID.x] = VkDrawMeshTasksIndirectCommandEXT(DrawItemCount[invocationID], 1, 1); }
