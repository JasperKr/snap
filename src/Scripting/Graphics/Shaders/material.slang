module material;

public struct MaterialData {
  public float4 albedoFactor;
  public float roughnessFactor;
  public float metallicFactor;
  public float reflectanceFactor;
  public float3 emissiveFactor;
  public uint alphaMode;
  public float alphaCutoff;
  public uint2 textureUVIndices;
};

public enum AlphaMode {
  Opaque = 0,
  Mask = 1,
  Blend = 2,
};

public uniform StructuredBuffer<MaterialData>.Handle MaterialBuffer;

