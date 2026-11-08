module helpers;

import constants;

const static int IBLIntegrationImportanceSamplingCount = 64;
const static int SpecularPrefilteringMipCount = 6;
const static int VolumetricCubemapMipCount = 5;

#define CUSTOM_OCT_SAMPLING 1

public float perceptualRoughnessToLod(float perceptualRoughness) {
  float roughness = perceptualRoughness * perceptualRoughness;
  float roughnessSqr = clamp(roughness * roughness, 0.0001, 1.0);

  float gloss = log2(2.0 / roughnessSqr - 1.0) * (1.0 / 18.0);

  float lod = (1.0 - gloss) * SpecularPrefilteringMipCount;

  return clamp(lod, 0.0, SpecularPrefilteringMipCount);
}

public float2 hammersley(uint index) {
  const uint numSamples = uint(IBLIntegrationImportanceSamplingCount);
  const float invNumSamples = 1.0 / float(numSamples);
  const float tof = 0.5 / float(0x80000000U);
  uint bits = index;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float2(float(index) * invNumSamples, float(bits) * tof);
}

public float3 importanceSamplingNdfDggx(float2 u, float roughness) {
  // Importance sampling D_GGX
  float a2 = roughness * roughness;
  float phi = 2.0 * PI * u.x;
  float cosTheta2 = (1.0 - u.y) / (1.0 + (a2 - 1.0) * u.y);
  float cosTheta = sqrt(cosTheta2);
  float sinTheta = sqrt(1.0 - cosTheta2);
  return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

public float3 hemisphereCosSample(float2 u) {
  float phi = 2.0 * PI * u.x;
  float cosTheta2 = 1.0 - u.y;
  float cosTheta = sqrt(cosTheta2);
  float sinTheta = sqrt(1.0 - cosTheta2);
  return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

public float3 importanceSamplingVNdfDggx(float2 u, float roughness, float3 v) {
  // See: "A Simpler and Exact Sampling Routine for the GGX Distribution of Visible Normals", Eric Heitz
  float alpha = roughness;

  // stretch view
  v = normalize(float3(alpha * v.x, alpha * v.y, v.z));

  // orthonormal basis
  float3 up = abs(v.z) < 0.9999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
  float3 t = normalize(cross(up, v));
  float3 b = cross(t, v);

  // sample point with polar coordinates (r, phi)
  float a = 1.0 / (1.0 + v.z);
  float r = sqrt(u.x);
  float phi = (u.y < a) ? u.y / a * PI : PI + (u.y - a) / (1.0 - a) * PI;
  float p1 = r * cos(phi);
  float p2 = r * sin(phi) * ((u.y < a) ? 1.0 : v.z);

  // compute normal
  float3 h = p1 * t + p2 * b + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * v;

  // unstretch
  h = normalize(float3(alpha * h.x, alpha * h.y, max(0.0, h.z)));
  return h;
}

public float prefilteredImportanceSampling(float ipdf, float omegaP) {
  // See: "Real-time Shading with Filtered Importance Sampling", Jaroslav Krivanek
  // Prefiltering doesn't work with anisotropy
  const float numSamples = float(IBLIntegrationImportanceSamplingCount);
  const float invNumSamples = 1.0 / float(numSamples);
  const float K = 4.0;
  float omegaS = invNumSamples * ipdf;
  float mipLevel = log2(K * omegaS / omegaP) * 0.5; // log4
  return mipLevel;
}

float signNotZero(float x) { return x < 0.0 ? -1.0 : 1.0; }

float2 signNotZero(float2 v) { return float2(signNotZero(v.x), signNotZero(v.y)); }

float3 signNotZero(float3 v) { return float3(signNotZero(v.x), signNotZero(v.y), signNotZero(v.z)); }

bool OctFlipped(float2 r) {
  float t = max(abs(r.x), abs(r.y));
  return t >= 1.0;
}

float2 OctFlip(float2 coord) {
  // coord = OctFlipped(coord) ? -coord : coord;
  return coord * 0.5 + 0.5;
}

int2 OctWrap(int2 size, int2 uv) {
  if (uv.x < 0 || uv.x >= size.x)
    uv.y = (size.y - 1) - uv.y;

  if (uv.y < 0 || uv.y >= size.y)
    uv.x = (size.x - 1) - uv.x;

  uv = clamp(uv, int2(0), size - 1);

  return uv;
}

public float2 Oct3To2(float3 n) {
  float tx, ty;
  bool neg;
  tx = abs(n.x) + abs(n.y);
  tx = tx + abs(n.z);
  tx = 1.0 / tx;
  n.x = n.x * tx;
  n.y = n.y * tx;
  tx = 1.0 - abs(n.y);
  neg = n.x < 0.0;
  tx = neg ? -tx : tx;
  ty = 1.0 - abs(n.x);
  neg = n.y < 0.0;
  ty = neg ? -ty : ty;
  neg = n.z <= 0.0;
  n.x = neg ? tx : n.x;
  n.y = neg ? ty : n.y;
  return n.xy;
}

public float2 EncodeOctahedron(float3 v) {
  float2 base = Oct3To2(v);
  return (base * 0.5 + 0.5);
}

public float3 DecodeOctahedron(float2 e) {
  e = e * 2.0 - float2(1.0);
  float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0.0)
    v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

public float3 textureOct(Sampler2DArray<float3> tex, float3 direction, int index, float mip) {
  float2 uv = OctFlip(Oct3To2(direction));

#if CUSTOM_OCT_SAMPLING
  int lower = int(floor(mip));

  int2 sizeIL;
  int2 sizeIU;
  int2 unused;

  tex.GetDimensions(lower, sizeIL.x, sizeIL.y, unused.x, unused.y);
  tex.GetDimensions(lower + 1, sizeIU.x, sizeIU.y, unused.x, unused.y);

  float2 sizeL = float2(sizeIL);
  float2 sizeU = float2(sizeIU);

  float2 baseL = floor(uv * sizeL - 0.5);
  float2 baseU = floor(uv * sizeU - 0.5);

  float2 fracL = uv * sizeL - baseL - 0.5;
  float2 fracU = uv * sizeU - baseU - 0.5;
  float lodFrac = fract(mip);

  int2 coordL = int2(baseL);
  int2 coordU = int2(baseU);

  float3 sumA = lerp(tex.Load(int4(OctWrap(sizeIL, coordL), index, lower)), tex.Load(int4(OctWrap(sizeIL, coordL + int2(1, 0)), index, lower)), fracL.x);

  float3 sumB = lerp(tex.Load(int4(OctWrap(sizeIL, coordL + int2(0, 1)), index, lower)), tex.Load(int4(OctWrap(sizeIL, coordL + 1), index, lower)), fracL.x);

  float3 sumLower = lerp(sumA, sumB, fracL.y);

  sumA = lerp(tex.Load(int4(OctWrap(sizeIU, coordU), index, lower + 1)), tex.Load(int4(OctWrap(sizeIU, coordU + int2(1, 0)), index, lower + 1)), fracU.x);

  sumB = lerp(tex.Load(int4(OctWrap(sizeIU, coordU + int2(0, 1)), index, lower + 1)), tex.Load(int4(OctWrap(sizeIU, coordU + 1), index, lower + 1)), fracU.x);

  float3 sumUpper = lerp(sumA, sumB, fracU.y);

  return lerp(sumLower, sumUpper, lodFrac);

#else
  return tex.SampleLevel(float3(uv, index), mip);
#endif
}

public float3 textureOct(Sampler2D<float3> tex, float3 direction, float mip) {
  float2 uv = OctFlip(Oct3To2(direction));

#if CUSTOM_OCT_SAMPLING
  int lower = int(floor(mip));

  int2 sizeIL;
  int2 sizeIU;
  int unused;

  tex.GetDimensions(lower, sizeIL.x, sizeIL.y, unused);
  tex.GetDimensions(lower + 1, sizeIU.x, sizeIU.y, unused);

  float2 sizeL = float2(sizeIL);
  float2 sizeU = float2(sizeIU);

  float2 baseL = floor(uv * sizeL - 0.5);
  float2 baseU = floor(uv * sizeU - 0.5);

  float2 fracL = uv * sizeL - baseL - 0.5;
  float2 fracU = uv * sizeU - baseU - 0.5;
  float lodFrac = fract(mip);

  int2 coordL = int2(baseL);
  int2 coordU = int2(baseU);

  float3 sumA = lerp(tex.Load(int3(OctWrap(sizeIL, coordL), lower)), tex.Load(int3(OctWrap(sizeIL, coordL + int2(1, 0)), lower)), fracL.x);

  float3 sumB = lerp(tex.Load(int3(OctWrap(sizeIL, coordL + int2(0, 1)), lower)), tex.Load(int3(OctWrap(sizeIL, coordL + 1), lower)), fracL.x);

  float3 sumLower = lerp(sumA, sumB, fracL.y);

  sumA = lerp(tex.Load(int3(OctWrap(sizeIU, coordU), lower + 1)), tex.Load(int3(OctWrap(sizeIU, coordU + int2(1, 0)), lower + 1)), fracU.x);

  sumB = lerp(tex.Load(int3(OctWrap(sizeIU, coordU + int2(0, 1)), lower + 1)), tex.Load(int3(OctWrap(sizeIU, coordU + 1), lower + 1)), fracU.x);

  float3 sumUpper = lerp(sumA, sumB, fracU.y);

  return lerp(sumLower, sumUpper, lodFrac);

#else
  return tex.SampleLevel(uv, mip);
#endif
}

public float3 getReflectedVector(const float3 v, const float3 n) { return reflect(-v, n); }

public float3 prefilteredRadiance(SamplerCube env, const float3 r, float perceptualRoughness) {
  float lod = perceptualRoughnessToLod(perceptualRoughness);
  return env.SampleLevel(r, lod).rgb;
}

public float3 prefilteredRadiance(Sampler2DArray<float3> env, const float3 r, float perceptualRoughness) {
  float lod = perceptualRoughnessToLod(perceptualRoughness);
  return env.SampleLevel(r, lod);
}

public float3 prefilteredRadiance(Sampler2DArray<float3> env, const float4 r, float perceptualRoughness) {
  float lod = perceptualRoughnessToLod(perceptualRoughness);
  return textureOct(env, r.xyz, int(r.w), lod);
}

public float3 prefilteredIrradiance(Sampler2DArray<float3> env, const float4 r) { return textureOct(env, r.xyz, int(r.w), 0.0).rgb; }

public float mipToAnisotropy(int mip) { return lerp(0.95, 0.0, float(mip) / float(VolumetricCubemapMipCount - 1)); }

public float anisotropyToMip(float anisotropy) { return (1.0 - saturate(anisotropy / 0.95)) * float(VolumetricCubemapMipCount - 1); }

public float3 prefilteredVolumetricRadiance(Sampler2DArray<float3> RadianceInput, const float3 dir, const int index, float anisotropy) {
  float mip = anisotropyToMip(clamp(anisotropy, 0.0, 0.95));
  return textureOct(RadianceInput, dir, index, mip).rgb;
}
