module volume;

public struct Volume {
  public float density;
  public float anisotropy;

  public Sampler3D<float>.Handle texture;

  public float3 boundsMin;
  uint padding_0;
  public float3 boundsMax;
  uint padding_1;

  public bool HasTexture() { return uint2(texture).x != 0; }

  public bool ContainsPoint(float3 position) {
    return all(position >= boundsMin) && all(position <= boundsMax);
  }

  public float3 ToUVW(float3 position) {
    return (position - boundsMin) / (boundsMax - boundsMin);
  }

  public float SampleDensity(float3 samplePosition) {
    if (!ContainsPoint(samplePosition)) {
      return 0.0F;
    }

    if (!HasTexture()) {
      return density;
    }

    float3 position = ToUVW(samplePosition);
    return texture.Sample(position) * density;
  }

  // Need to be normalised
  public float GetAnisotropy(float3 viewDirection, float3 lightDirection) {
    return GetAnisotropy(dot(viewDirection, lightDirection));
  }

  // View dot Light
  public float GetAnisotropy(float VoL) {
    return PhaseFunction(VoL, anisotropy);
  }

  // Henyey-Greenstein phase function approximated with spherical harmonics
  public static float PhaseFunction(float cosTheta, float g) {
    // SH Coefficients for first-order HG approximation
    float g2 = g * g;
    float denom = 1.0F + g2 - 2.0F * g * cosTheta;

    // return (1.0 - g2) / (denom * sqrt(denom));
    return (1.0 - g2) * rsqrt(denom * denom * denom); // x*sqrt(x) = sqrt(x*x*x)
  }

  public bool TraceRay(RayDesc ray, out float t0, out float t1) {
    float3 _t0 = (boundsMin - ray.Origin) * ray.Direction;
    float3 _t1 = (boundsMax - ray.Origin) * ray.Direction;

    float3 tmin = min(_t0, _t1);
    float3 tmax = max(_t0, _t1);

    t0 = max(max(tmin.x, tmin.y), tmin.z);
    t1 = min(min(tmax.x, tmax.y), tmax.z);

    return t1 > 0.0F;
  }
};
