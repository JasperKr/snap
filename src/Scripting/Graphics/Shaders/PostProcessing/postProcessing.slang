
float3 agxDefaultContrastApprox(float3 x) {
  float3 x2 = x * x;
  float3 x4 = x2 * x2;
  return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x +
         0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 agx(float3 val) {
  static const float3x3 agx_mat =
      float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
               0.0784335999999992, 0.878468636469772, 0.0784336,
               0.0792237451477643, 0.0791661274605434, 0.879142973793104);
  static const float minEv = -12.47393;
  static const float maxEv = 4.026069;

  // Input transform
  val = mul(val, agx_mat);

  // Log2 space encoding
  val = clamp(log2(val), minEv, maxEv);
  val = (val - minEv) / (maxEv - minEv);

  // Apply sigmoid function approximation
  val = agxDefaultContrastApprox(val);

  return val;
}

static const float3x3 inv_agx_mat =
    float3x3(1.1968790051202, -0.052896851757456, -0.052971635514444,
             -0.098020881140137, 1.1519031299042, -0.098043450117124,
             -0.09902974407972, -0.098961176844843, 1.1510736726412);
float3 inv_agx(float3 val) {
  // Input transform
  val = mul(val, inv_agx_mat);
  return val;
}

uniform int LOOK;
uniform float brightness;

float3 agxLook(float3 val) {
  static const float3 lw = float3(0.2126, 0.7152, 0.0722);
  float luma = dot(val, lw);

  float3 offset = float3(brightness, brightness, brightness);
  float3 slope;
  float3 power;
  float sat;

  if (LOOK == 1) {
    // Golden
    slope = float3(1.0, 0.9, 0.5);
    power = float3(0.8, 0.8, 0.8);
    sat = 0.8;

    val = pow(val * slope + offset, power);
    return luma + sat * (val - luma);
  } else if (LOOK == 2) {
    // Punchy
    power = float3(1.35, 1.35, 1.35);
    sat = 1.2;

    val = pow(val + offset, power);
    return luma + sat * (val - luma);
  } else {
    // Linear
    val += offset;
    return luma + (val - luma);
  }
}

//https://docs.unity3d.com/Packages/com.unity.shadergraph@6.9/manual/White-Balance-Node.html
float3 whiteBalance(float3 In, float Temperature, float Tint) {
  // Range ~[-1.67;1.67] works best
  float t1 = Temperature * 10 / 6;
  float t2 = Tint * 10 / 6;

  // Get the CIE xy chromaticity of the reference white point.
  // Note: 0.31271 = x value on the D65 white point
  float x = 0.31271 - t1 * (t1 < 0 ? 0.1 : 0.05);
  float standardIlluminantY = 2.87 * x - 3 * x * x - 0.27509507;
  float y = standardIlluminantY + t2 * 0.05;

  // Calculate the coefficients in the LMS space.
  float3 w1 = float3(0.949237, 1.03542, 1.08728); // D65 white point

  // CIExyToLMS
  float Y = 1;
  float X = Y * x / y;
  float Z = Y * (1 - x - y) / y;
  float L = 0.7328 * X + 0.4296 * Y - 0.1624 * Z;
  float M = -0.7036 * X + 1.6975 * Y + 0.0061 * Z;
  float S = 0.0030 * X + 0.0136 * Y + 0.9834 * Z;
  float3 w2 = float3(L, M, S);

  float3 balance = float3(w1.x / w2.x, w1.y / w2.y, w1.z / w2.z);

  float3x3 LIN_2_LMS_MAT = { 3.90405e-1, 5.49941e-1, 8.92632e-3,
                             7.08416e-2, 9.63172e-1, 1.35775e-3,
                             2.31082e-2, 1.28021e-1, 9.36245e-1 };

  float3x3 LMS_2_LIN_MAT = { 2.85847e+0,  -1.62879e+0, -2.48910e-2,
                             -2.10182e-1, 1.15820e+0,  3.24281e-4,
                             -4.18120e-2, -1.18169e-1, 1.06867e+0 };

  float3 lms = mul(In, LIN_2_LMS_MAT);
  lms *= balance;
  return mul(lms, LMS_2_LIN_MAT);
}

// https://docs.unity3d.com/Packages/com.unity.shadergraph@6.9/manual/Saturation-Node.html
float3 ApplySaturation(float3 In, float Saturation) {
  float luma = dot(In, float3(0.2126729, 0.7151522, 0.0721750));
  return luma.xxx + Saturation.xxx * (In - luma.xxx);
}

// Fog>Exposure>Bloom>Chromatic Aberration>White Balancing>Contrast>Brightness>Color filtering>Saturation>Tone mapping
float3 ApplyConstrast(float3 val, float contrast) {
  float3 mid = float3(0.5, 0.5, 0.5);
  return (val - mid) * contrast + mid;
}

float3 ApplyVignette(float3 val, float2 uv, float vignette) {
  float2 centeredUV = uv - 0.5;
  float dist = length(centeredUV);
  float vig = smoothstep(0.8, 0.5, dist);
  return lerp(val, val * vig, vignette);
}

struct FragmentInput {
  float2 tex_coord : TEXCOORDS;
};

Sampler2D IncomingLight;
uniform float Temperature;
uniform float Tint;
uniform bool ApplyAGX;
uniform float Contrast;
uniform float Saturation;
uniform float Vignette;
uniform float Exposure;

import utils;

#include "fullscreen.slang"

[shader("fragment")]
float4 fragmentMain(FragmentInput input) {
  float3 incomingLight = Sanitise(IncomingLight.Sample(input.tex_coord).rgb);
  incomingLight *= Exposure;

  float3 color = whiteBalance(incomingLight, Temperature, Tint);

  if (ApplyAGX) {
    color = agx(color);
    color = agxLook(color);
    color = inv_agx(color);
  }

  color = ApplyConstrast(color, Contrast);
  color = ApplySaturation(color, Saturation);
  color = ApplyVignette(color, input.tex_coord, Vignette);

  return float4(color, 1.0);
}
