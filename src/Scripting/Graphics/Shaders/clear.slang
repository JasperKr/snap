
uniform WTexture2D.Handle ClearTexture;
uniform float4 ClearValue;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint2 invocationID: SV_DispatchThreadID) {
  uint2 dimensions;
  ClearTexture.GetDimensions(dimensions.x, dimensions.y);

  if (all(invocationID < dimensions))
    ClearTexture.Store(invocationID, ClearValue);
}
