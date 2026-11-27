module meshletPayload;

public struct TaskPayload {
  public uint MeshletCount;
  public uint32_t[64] MeshIndices;
  public uint16_t[64] MeshletOffset;
};
