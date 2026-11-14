module camera;

public enum ProjectionType {
  Perspective,
  Orthographic,
};

public struct _CameraData {
  public float4x4 ViewMatrix;
  public float4x4 InverseViewMatrix;
  public float4x4 ProjectionMatrix;
  public float4x4 InverseProjectionMatrix;
  public float4x4 ViewProjectionMatrix;
  public float4x4 InverseViewProjectionMatrix;
  public float4x4 RotationProjectionMatrix;
  public float4x4 InverseRotationProjectionMatrix;

  public float3 Position;
  public float Near;

  public float Far;
  public float NearMulFar;
  public float FarMinusNear;

  // [Projection type: 0-0]
  // [History invalidated 1-1]
  // [Cascade count 2-9]
  // [Specular enabled 10-10]
  uint Settings;

  public float2 Jitter;

  public float3 GetForward() { return -normalize(ViewMatrix[2].xyz); }
  public float3 GetRight() { return normalize(ViewMatrix[0].xyz); }
  public float3 GetUp() { return normalize(ViewMatrix[1].xyz); }

  public float GetDistance(float2 uvs, float depth) { return distance(GetWorldPosition(uvs, depth), Position); }

  public float3 GetWorldPosition(float2 uv, float depth) {
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 worldPos = mul(clipPos, InverseViewProjectionMatrix);
    return worldPos.xyz / worldPos.w;
  }

  public float3 GetRayDirection(float2 uv) {
    float4 clipPos = float4(uv * 2.0 - 1.0, 1.0, 1.0);
    float4 worldPos = mul(clipPos, InverseRotationProjectionMatrix);
    return normalize(worldPos.xyz);
  }

  public float3 GetPreviousNDC(float3 worldPos) {
    float4 clipPos = mul(CameraData[1].ViewProjectionMatrix, float4(worldPos, 1.0));
    return clipPos.xyz / clipPos.w;
  }

  public float3 ReprojectToPreviousNDC(float2 uv, float depth) {
    float3 worldPos = GetWorldPosition(uv, depth);
    return GetPreviousNDC(worldPos);
  }

  public float3 GetPreviousUVs(float3 worldPos) {
    float4 clipPos = mul(CameraData[1].ViewProjectionMatrix, float4(worldPos, 1.0));
    clipPos.xyz /= clipPos.w;
    return clipPos.xyz * 0.5f + 0.5f;
  }

  public float3 ReprojectToPreviousUVs(float2 uv, float depth) {
    float3 worldPos = GetWorldPosition(uv, depth);
    return GetPreviousUVs(worldPos);
  }

  public ProjectionType GetProjectionType() { return ProjectionType(Settings & 1); }
  public bool HistoryInvalidated() { return (Settings & 2) != 0; }
  public uint GetShadowCascadeCount() { return (Settings >> 2) & 8; }
  public bool SpecularEnabled() { return (Settings & (1 << 10)) != 0; }
};

public uniform StructuredBuffer<_CameraData>.Handle CameraData;

// Reverse-Z
public static const float DepthFarPlane = 0.0;
public static const float DepthNearPlane = 1.0;
