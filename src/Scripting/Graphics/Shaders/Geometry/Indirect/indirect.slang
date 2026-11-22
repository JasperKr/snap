

static const uint DrawLayers = 3U;         // Opaque, Masked, Blend
static const uint PipelineStateCount = 3U; // Only varies in cull mode. Front, Back, None

struct VkDrawIndirectCommand {
  uint32_t vertexCount;
  uint32_t instanceCount;
  uint32_t firstVertex;
  uint32_t firstInstance;
};

uniform RWStructuredBuffer<VkDrawIndirectCommand>.Handle DrawInfo[DrawLayers * PipelineStateCount];
uniform StructuredBuffer<uint>.Handle DrawItemCount;

[shader("compute")]
[numthreads(DrawLayers * PipelineStateCount, 1, 1)]
void computeMain(uint3 invocationID: SV_DispatchThreadID) { DrawInfo[invocationID.x][0] = VkDrawIndirectCommand(DrawItemCount[invocationID.x], 1, 0, 0); }
