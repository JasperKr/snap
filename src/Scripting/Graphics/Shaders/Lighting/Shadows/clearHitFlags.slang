uniform WTexture2D<uint>.Handle OutputTexture;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  uint width;
  uint height;
  OutputTexture.GetDimensions(width, height);

  id.xy *= 4U;

  if (id.x >= width || id.y >= height) {
    return;
  }

  uint xLimit = min(width, id.x + 4U);
  uint yLimit = min(height, id.y + 4U);

  for (int y = id.y; y < yLimit; ++y) {
    for (int x = id.x; x < xLimit; ++x) {
      OutputTexture.Store(uint2(x, y), 0);
    }
  }
}
