module random;

import constants;

public uniform Sampler2D.Handle BlueNoiseTexture;

public float RandomValue(inout uint state) {
  state = state * 747796405u + 2891336453u;
  uint result = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  result = (result >> 22u) ^ result;
  return float(result) / 4294967295.0;
}

public float RandomValue05(inout uint state) {
  state = state * 747796405u + 2891336453u;
  uint result = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  result = (result >> 22u) ^ result;
  return float(result) * (1.0 / 4294967295.0) + (-0.5);
}

public float RandomValueNorm(inout uint state) {
  state = state * 747796405u + 2891336453u;
  uint result = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  result = (result >> 22u) ^ result;
  return float(result) * (2.0 / 4294967295.0) - 1.0;
}

public float RandomNormalDistributionValue(inout uint state) {
  float t = TWO_PI * RandomValue(state);
  float r = sqrt(-2.0 * log(RandomValue(state)));
  return r * cos(t);
}

public float2 RandomDirection2D(inout uint state) {
  float phi = TWO_PI * RandomValue(state);
  float s, c;
  sincos(phi, s, c);
  return float2(c, s);
}

public float2 RandomPointOnUnitDisc2D(float rnd0, float rnd1) {
  float r = sqrt(rnd0);
  float phi = TWO_PI * rnd1;
  float s, c;
  sincos(phi, s, c);
  return float2(r * c, r * s);
}

public float2 RandomPointOnUnitDisc2D(inout uint state) {
  float rnd0 = RandomValue(state);
  float rnd1 = RandomValue(state);

  return RandomPointOnUnitDisc2D(rnd0, rnd1);
}

public float3 RandomDirection(inout uint state) {
  float z = 2.0 * RandomValue(state) - 1.0;
  float phi = TWO_PI * RandomValue(state);

  float r = sqrt(1.0 - z * z);
  float s, c;
  sincos(phi, s, c);

  return float3(r * c, r * s, z);
}

public float2 RandomValue2D(inout uint state) {
  return float2(RandomValueNorm(state), RandomValueNorm(state));
}

public float3 RandomValue3D(inout uint state) {
  return float3(RandomValueNorm(state), RandomValueNorm(state),
                RandomValueNorm(state));
}

public float3 RandomHemisphereDirection(float3 normal, uint state) {
  float3 dir = RandomDirection(state);
  return dir * sign(dot(normal, dir));
}

public float3 RandomHemisphereDirection(float3 normal, float rnd0, float rnd1) {
  float z = 2.0 * rnd0 - 1.0;
  float phi = TWO_PI * rnd1;

  float r = sqrt(1.0 - z * z);
  float s, c;
  sincos(phi, s, c);

  float3 dir = float3(r * c, r * s, z);
  return dir * sign(dot(normal, dir));
}

public float2 RandomHemisphereDirection(float2 normal, uint state) {
  float2 dir = RandomDirection2D(state);
  return dir * sign(dot(normal, dir));
}
