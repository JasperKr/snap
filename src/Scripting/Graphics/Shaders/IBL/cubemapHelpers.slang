module cubemapHelpers;

// level -1 means out of bounds
// uv is in the range [-1, 1]
// id.x is the texel index in the range [0, sum of all texels in all mipmaps)
// id output is in the range [0, BASE-1] for x and y and z stores the mip level
public void getTexelInfo(inout int3 id, out int level, out float2 uv, const int BASE) {

  const int MIP0 = BASE * BASE;
  const int MIP1 = MIP0 + (BASE >> 1) * (BASE >> 1);
  const int MIP2 = MIP1 + (BASE >> 2) * (BASE >> 2);
  const int MIP3 = MIP2 + (BASE >> 3) * (BASE >> 3);
  const int MIP4 = MIP3 + (BASE >> 4) * (BASE >> 4);
  const int MIP5 = MIP4 + (BASE >> 5) * (BASE >> 5);
  const int MIP6 = MIP5 + (BASE >> 6) * (BASE >> 6);

  // determine which texel this is
  if (id.x < MIP0)
    level = 0;
  else if (id.x < MIP1) {
    level = 1;
    id.x -= MIP0;
  } else if (id.x < MIP2) {
    level = 2;
    id.x -= MIP1;
  } else if (id.x < MIP3) {
    level = 3;
    id.x -= MIP2;
  } else if (id.x < MIP4) {
    level = 4;
    id.x -= MIP3;
  } else if (id.x < MIP5) {
    level = 5;
    id.x -= MIP4;
  } else if (id.x < MIP6) {
    level = 6;
    id.x -= MIP5;
  } else {
    level = -1;
    uv = float2(0.0, 0.0);
    return;
  }

  id.z = id.y;
  int res = BASE >> level;
  id.y = id.x / res;
  id.x -= id.y * res;

  uv.x = (float(id.x) * 2.0 + 1.0) / float(res) - 1.0;
  uv.y = -(float(id.y) * 2.0 + 1.0) / float(res) + 1.0;
}

public void get_dir(out float3 dir, float2 uv, int face) {
  switch (face) {
  case 0:
    dir[0] = 1;
    dir[1] = uv[1];
    dir[2] = -uv[0];
    break;
  case 1:
    dir[0] = -1;
    dir[1] = uv[1];
    dir[2] = uv[0];
    break;
  case 2:
    dir[0] = uv[0];
    dir[1] = 1;
    dir[2] = -uv[1];
    break;
  case 3:
    dir[0] = uv[0];
    dir[1] = -1;
    dir[2] = uv[1];
    break;
  case 4:
    dir[0] = uv[0];
    dir[1] = uv[1];
    dir[2] = 1;
    break;
  default:
    dir[0] = -uv[0];
    dir[1] = uv[1];
    dir[2] = -1;
    break;
  }
}
