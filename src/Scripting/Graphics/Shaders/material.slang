module material;

public struct MaterialData {
  public float4 albedoFactor;

  public float roughnessFactor;
  public float metallicFactor;
  public float reflectanceFactor;
  uint padding0;

  public float3 emissiveFactor;
  public uint alphaMode;

  public float alphaCutoff;
  uint padding1;
  public uint2 textureUVIndices;

  public Sampler2D<float4>.Handle AlbedoTexture;
  public Sampler2D<float4>.Handle NormalTexture;
  public Sampler2D<float4>.Handle MetallicRoughnessTexture;
  public Sampler2D<float>.Handle AmbientOcclusionTexture;
  public Sampler2D<float>.Handle ReflectanceTexture;
  public Sampler2D<float4>.Handle EmissiveTexture;

  public bool HasAlbedoTexture() { return uint2(AlbedoTexture).x != 0; }
  public bool HasNormalTexture() { return uint2(NormalTexture).x != 0; }
  public bool HasMetallicRoughnessTexture() { return uint2(MetallicRoughnessTexture).x != 0; }
  public bool HasAmbientOcclusionTexture() { return uint2(AmbientOcclusionTexture).x != 0; }
  public bool HasReflectanceTexture() { return uint2(ReflectanceTexture).x != 0; }
  public bool HasEmissiveTexture() { return uint2(EmissiveTexture).x != 0; }

  public float4 AlbedoOrDefault(float2 uv) { return HasAlbedoTexture() ? AlbedoTexture.Sample(uv) : float4(1.0); }
  public float3 NormalOrDefault(float2 uv) { return HasNormalTexture() ? NormalTexture.Sample(uv).xyz : float3(0.5, 0.5, 1.0); }
  public float2 MetallicRoughnessOrDefault(float2 uv) { return HasMetallicRoughnessTexture() ? MetallicRoughnessTexture.Sample(uv).bg : float2(1.0); }
  public float AmbientOcclusionOrDefault(float2 uv) { return HasAmbientOcclusionTexture() ? AmbientOcclusionTexture.Sample(uv).r : float(1.0); }
  public float ReflectanceOrDefault(float2 uv) { return HasReflectanceTexture() ? ReflectanceTexture.Sample(uv).r : float(1.0); }
  public float3 EmissiveOrDefault(float2 uv) { return HasEmissiveTexture() ? EmissiveTexture.Sample(uv).rgb : float3(1.0); }
};

public enum AlphaMode {
  Opaque = 0,
  Mask = 1,
  Blend = 2,
};

public uniform StructuredBuffer<MaterialData>.Handle MaterialBuffer;
