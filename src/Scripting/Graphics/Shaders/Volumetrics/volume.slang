module volume;

public struct Volume {
  public float density;
  public float anisotropy;

  public Sampler3D<float>.Handle densityTexture;
  public Sampler3D<float4>.Handle colorTexture;

  uint pad_0;
  uint pad_1;

  public float4 boundsMin; // W is unused
  public float4 boundsMax; // W is unused

  public float4 color; // W is unused

  public bool HasDensityTexture() { return uint2(densityTexture).x != 0; }

  public bool HasColorTexture() { return uint2(colorTexture).x != 0; }

  public bool ContainsPoint(float3 position) { return all(position >= boundsMin.xyz) && all(position <= boundsMax.xyz); }

  public float3 ToUVW(float3 position) { return (position - boundsMin.xyz) / (boundsMax.xyz - boundsMin.xyz); }

  public float SampleDensity(float3 samplePosition) {
    if (!ContainsPoint(samplePosition)) {
      return 0.0F;
    }

    if (!HasDensityTexture()) {
      return density;
    }

    float3 position = ToUVW(samplePosition);
    return densityTexture.SampleLevelZero(position) * density;
  }

  public float3 GetColor(float3 samplePosition) {
    if (!ContainsPoint(samplePosition)) {
      return float3(1.0F);
    }

    if (!HasColorTexture()) {
      return color.rgb;
    }

    float3 position = ToUVW(samplePosition);
    return colorTexture.SampleLevelZero(position).rgb;
  }

  // Need to be normalised
  public float GetAnisotropy(float3 viewDirection, float3 lightDirection) { return GetAnisotropy(dot(viewDirection, lightDirection)); }

  // View dot Light
  public float GetAnisotropy(float VoL) { return PhaseFunction(VoL, anisotropy); }

  // Henyey-Greenstein phase function approximated with spherical harmonics
  public static float PhaseFunction(float cosTheta, float g) {
    // SH Coefficients for first-order HG approximation
    float g2 = g * g;
    float denom = 1.0F + g2 - 2.0F * g * cosTheta;

    // return (1.0 - g2) / (denom * sqrt(denom));
    return (1.0 - g2) * rsqrt(denom * denom * denom); // x*sqrt(x) = sqrt(x*x*x)
  }

  public bool TraceRay(RayDesc ray, out float t0, out float t1) {
    float3 _t0 = (boundsMin.xyz - ray.Origin) / ray.Direction;
    float3 _t1 = (boundsMax.xyz - ray.Origin) / ray.Direction;

    float3 tmin = min(_t0, _t1);
    float3 tmax = max(_t0, _t1);

    t0 = max(max(tmin.x, tmin.y), tmin.z);
    t1 = min(min(tmax.x, tmax.y), tmax.z);

    return t1 > 0.0F;
  }
};
