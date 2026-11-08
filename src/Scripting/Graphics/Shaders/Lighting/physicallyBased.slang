module physicallyBased;

import constants;

public float power5(float x) {
  float x2 = x * x;
  return x2 * x2 * x;
}

public float Distribution_GGX(float NoH, float roughness) {
  float oneMinusNoH2 = 1.0 - NoH * NoH;

  float a = NoH * roughness;
  float k = roughness / (oneMinusNoH2 + a * a);
  float d = k * k * INV_PI;
  return min(d, 65534.0);
}

public float3 Fresnel_Schlick(const float3 Fresnel0, float Fresnel90, float VoH) { return Fresnel0 + (Fresnel90 - Fresnel0) * power5(1.0 - VoH); }

// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/brdf.hlsli
public float Visibility_SmithGGXCorrelated(float roughness, float NoV, float NoL, float lambdaV) {
  float rSqr = roughness * roughness;
  float lambdaL = NoV * sqrt((NoL - rSqr * NoL) * NoL + rSqr);
  float visibility = 0.5 / max(lambdaV * NoL + lambdaL, 1e-5);
  return min(visibility, 65534.0);
}

public float3 IsotropicLobe(float roughness, float3 F0, float NoV, float NoL, float NoH, float LoH, float lambdaV) {
  float D = Distribution_GGX(NoH, roughness);
  float V = Visibility_SmithGGXCorrelated(roughness, NoV, NoL, lambdaV);
  float3 F = Fresnel_Schlick(F0, 1.0, LoH);
  return (D * V) * F;
}

public float3 DiffuseLobe(float3 diffuse) { return diffuse * INV_PI; }

public float3 ComputeFresnel0(float3 albedo, float metallic, float reflectance) {
  float r = 0.16 * reflectance * reflectance;

  return lerp(float3(r, r, r), albedo, metallic);
}

public float3 mx_GGX_Dir_Albedo_Analytic(float NdotV, float alpha, float3 Fresnel0, float3 Fresnel90) {
  float x = NdotV;
  float y = alpha;
  float x2 = x * x;
  float y2 = y * y;

  float4 r = float4(0.1003, 0.9345, 1.0, 1.0) + float4(-0.6303, -2.323, -1.765, 0.2281) * x + float4(9.748, 2.229, 8.263, 15.94) * y +
             float4(-2.038, -3.748, 11.53, -55.83) * x * y + float4(29.34, 1.424, 28.96, 13.08) * x2 + float4(-8.245, -0.7684, -7.507, 41.26) * y2 +
             float4(-26.44, 1.436, -36.11, 54.9) * x2 * y + float4(19.99, 0.2913, 15.86, 300.2) * x * y2 + float4(-5.448, 0.6286, 33.37, -285.1) * x2 * y2;

  float2 AB = clamp(r.xy / r.zw, 0.0, 1.0);

  return Fresnel0 * AB.x + Fresnel90 * AB.y;
}

public float3 prefilteredDFG(float perceptualRoughness, float3 Fresnel0, float NoV) {
  return mx_GGX_Dir_Albedo_Analytic(NoV, perceptualRoughness, Fresnel0, float3(1.0, 1.0, 1.0));
}

public float3 ComputeDiffuseColor(float3 albedo, float metallic) { return albedo * (1.0 - metallic); }

public float3 BiDirectionalDistributionFunction(float NoV, float NoH, float LoH, float roughness, float3 Fresnel0, float3 Diffuse, float NoL, float lambdaV) {
  float3 IncomingSpecular = IsotropicLobe(roughness, Fresnel0, NoV, NoL, NoH, LoH, lambdaV);
  float3 IncomingDiffuse = DiffuseLobe(Diffuse);
  float3 BRDF = IncomingSpecular + IncomingDiffuse;
  return BRDF * NoL;
}

public float3 SampleBRDF(float3 viewDirection, float3 lightDirection, float NoV, float3 normal, float roughness, float3 fresnel0, float3 diffuse, float NoL,
                         float lambdaV) {
  float3 halfVector = normalize(viewDirection + lightDirection);
  float NoH = saturate(dot(normal, halfVector));
  float LoH = saturate(dot(lightDirection, halfVector));

  return BiDirectionalDistributionFunction(NoV, NoH, LoH, roughness, fresnel0, diffuse, NoL, lambdaV);
}

public float SpecularOcclusion(float NoV, float AO, float roughness) { return saturate(pow(NoV + AO, exp2(-16.0 * roughness - 1.0)) - 1.0 + AO); }

public float ComputeLambdaV(float roughness, float NoV) {
  float rSqr = roughness * roughness;
  return sqrt((NoV - rSqr * NoV) * NoV + rSqr);
}

public float ClampNoV(float NoV) { return max(NoV, 1e-4); }

public float ClampPerceptualRoughness(float perceptualRoughness) { return max(perceptualRoughness, 0.045); }

public float CalculateRoughness(float perceptualRoughness) { return max(perceptualRoughness * perceptualRoughness, 0.002025); }

public struct MaterialChannel {
  public float metallic;
  public float perceptualRoughness;
  public float reflectance;
  public float occlusion;

  public __init(float metallic, float perceptualRoughness, float reflectance, float occlusion) {
    this.metallic = metallic;
    this.perceptualRoughness = ClampPerceptualRoughness(perceptualRoughness);
    this.reflectance = reflectance;
    this.occlusion = occlusion;
  }

  public __init(float4 packed) {
    this.metallic = packed.r;
    this.perceptualRoughness = ClampPerceptualRoughness(packed.g);
    this.reflectance = packed.b;
    this.occlusion = packed.a;
  }

  public property float roughness {
    get { return CalculateRoughness(perceptualRoughness); }
  }

  public property float4 packed {
    get { return float4(metallic, perceptualRoughness, reflectance, occlusion); }
  }
};
