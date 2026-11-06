module Utils;

public half3 UnpackNormal(uint packedNormal) {
  uint x = (packedNormal >> 0) & 0x3FF;
  uint y = (packedNormal >> 10) & 0x3FF;
  uint z = (packedNormal >> 20) & 0x3FF;

  half3 normal;
  normal.x = half((float(x) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  normal.y = half((float(y) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  normal.z = half((float(z) * (1.0f / 1023.0f)) * 2.0 - 1.0);

  return normal;
}

public half4 UnpackTangent(uint packedTangent) {
  uint x = (packedTangent >> 0) & 0x3FF;
  uint y = (packedTangent >> 10) & 0x3FF;
  uint z = (packedTangent >> 20) & 0x3FF;

  half4 tangent;
  tangent.x = half((float(x) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  tangent.y = half((float(y) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  tangent.z = half((float(z) * (1.0f / 1023.0f)) * 2.0 - 1.0);
  tangent.w = half(packedTangent >> 30 & 1) * 2.0H - 1.0H; // Convert back to -1 or 1

  return tangent;
}

public float2 UnpackMotionVector(const float2 packedMotionVector) {

  float2 unsignedMotion = packedMotionVector * packedMotionVector;
  return unsignedMotion * sign(packedMotionVector);
}

public float2 PackMotionVector(float2 motionVector) {
  // Bias precision towards 0, as most motion vectors are small
  float2 unsignedMotion = sqrt(abs(motionVector));
  return unsignedMotion * sign(motionVector);
}

public float3 Rotate(float3 v, float4 q) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }

public float3 GetRayDirection(float2 uv, float4x4 inverseRotationProjection) {
  float4 clipSpacePos = float4(uv * 2.0 - 1.0, 1.0, 1.0);
  float4 worldSpacePos = mul(clipSpacePos, inverseRotationProjection);
  return normalize(worldSpacePos.xyz);
}

public float Luminance(float3 color) { return dot(color, float3(0.2126, 0.7152, 0.0722)); }

// we should default to float.maxValue, however that causes issues when storing in non-float32
// textures which will always be the case since that would be dumb, so we clamp to 1000.0f
// TODO: See if this a good default

public float3 Sanitise(float3 color) {
  if (any(isnan(color)))
    return float3(0.0, 0.0, 0.0);

  if (any(isinf(color)))
    return float3(1000.0);

  return color;
}

public float4 Sanitise(float4 color) {
  if (any(isnan(color)))
    return float4(0.0, 0.0, 0.0, 1.0);

  if (any(isinf(color)))
    return float4(float3(1000.0), 1.0);

  return color;
}

public float4 BlendColors(float4 first, float4 second) { return sqrt(first * first + second * second); }
