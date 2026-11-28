module drawlists;

import utils;
import constants;
import camera;
import Geometry.transform;
import material;

static const uint MaxLods = 8;

public struct Vertex {
  public float3 Position;
  uint Normal;

  public float2 TexCoords;
  uint Tangent;
  uint Color;

  public half3 GetNormal() { return UnpackNormal(Normal); }
  public half4 GetTangent() { return UnpackTangent(Tangent); }
  public half4 GetColor() { return UnpackColor(Color); }
};

public struct Meshlet {
  public uint VertexCount;
  public uint PrimitiveCount;

  public uint32_t Indices[64];
  public uint8_t3 LocalIndices[126];
};

// A mesh is part of one buffer, lod's are ranges into that buffer
// this is done as an optimisation, so a mesh can be part of a big buffer storing multiple meshes
public struct Lod {
  public StructuredBuffer<Vertex>.Handle Vertices;
  public StructuredBuffer<CullData>.Handle MeshletCullData;
  public StructuredBuffer<Meshlet>.Handle Meshlets;

  public uint2 LodMeshletRanges[MaxLods];

  // Range area for each lod, lod 0 is range [1 to v] and the rest is [v:N-1 to v]
  // The last lod MUST end at 0, thus lod 0 can be set to 0 to disable LOD's.
  // If a range is set to < 0, the object is culled.
  public float LodAreaRange[MaxLods];

  public float4 BoundsMin;
  public float4 BoundsMax;

  public float4 CalculateScreenSpaceBounds(float3 bmin, float3 bmax) {
    float4 minAndMax = float4(1.0, 1.0, 0.0, 0.0);

    float2 point = CameraData[0].ProjectToClip(float3(bmin.x, bmin.y, bmin.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmax.x, bmin.y, bmin.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmin.x, bmax.y, bmin.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmax.x, bmax.y, bmin.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmin.x, bmin.y, bmax.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmax.x, bmin.y, bmax.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmin.x, bmax.y, bmax.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    point = CameraData[0].ProjectToClip(float3(bmax.x, bmax.y, bmax.z)).xy;
    minAndMax.xy = min(minAndMax.xy, point);
    minAndMax.zw = max(minAndMax.zw, point);

    return minAndMax;
  }

  public float CalculateScreenSpaceArea(float3 bmin, float3 bmax) {
    float4 bounds = CalculateScreenSpaceBounds(bmin, bmax);
    float2 size = bounds.zw - bounds.xy;
    return saturate(size.x * size.y);
  }

  public int CalculateLODIndex(float3 bmin, float3 bmax) {
    float area = CalculateScreenSpaceArea(bmin, bmax);
    float previous = 1.0;

    for (int i = 0; i < MaxLods; i++) {
      float current = LodAreaRange[i];
      if (area <= previous && area >= current) {
        return i;
      }

      if (current < 0.0) {
        return -1;
      }
    }

    return -1;
  }
};

public struct Mesh {
  public Lod Lod;
  public uint MaterialIndex;
  public uint TransformIndex;

  public float4x4 GetTransform() { return ModelTransforms[TransformIndex].ModelMatrix; }
  public float4x4 GetNormalMatrix() { return ModelTransforms[TransformIndex].NormalMatrix; }
  public MaterialData GetMaterial() { return MaterialBuffer[MaterialIndex]; }
};

public struct DrawItem {
  public StructuredBuffer<Vertex>.Handle Vertices;
  public StructuredBuffer<Meshlet>.Handle Meshlets;

  public StructuredBuffer<CullData>.Handle MeshletCullData;
  uint2 padding0;

  public uint ArrayOffset;
  public uint MeshletCount;
  public uint PrefixOffset; // Offset into global draw array
  uint padding1;

  public uint MaterialIndex;
  public uint TransformIndex;
  uint padding2;
  uint padding3;

  public float4x4 GetTransform() { return ModelTransforms[TransformIndex].ModelMatrix; }
  public float4x4 GetNormalMatrix() { return ModelTransforms[TransformIndex].NormalMatrix; }
  public MaterialData GetMaterial() { return MaterialBuffer[MaterialIndex]; }
};

public struct CullData {
  uint4 Data;

  public half3 GetPosition() {
    half2 xy = unpackHalf2x16ToHalf(Data.x);
    half z = unpackHalf2x16ToHalf(Data.y).x;
    return half3(xy, z);
  }

  public half GetRadius() { return unpackHalf2x16ToHalf(Data.y).y; }
  public half4 GetCone() { return unpackSnorm4x8ToHalf(Data.z); }
};

public void TransformBounds(inout float3 bmin, inout float3 bmax, float4x4 transform) {
  float3 center = (bmin + bmax) * 0.5F;
  float3 extents = (bmax - bmin) * 0.5F;

  float3 worldCenter = mul(float4(center, 1.0F), transform).xyz;
  float3 worldExtents = mul(float4(extents, 0.0F), abs(transform)).xyz;

  bmin = worldCenter - worldExtents;
  bmax = worldCenter + worldExtents;
}
