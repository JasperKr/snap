

import constants;
import utils;

#include "fullscreen.slang"

Sampler2D<float3> IncomingLight;
Sampler2D<float3> PreviousIncomingLight;
Sampler2D<float2> MotionTexture;

// The following code is licensed under the MIT license: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
float3 SampleTextureCatmullRom(Sampler2D<float3> tex, float2 uv, float2 texSize,
                               float2 texelSize) {
  // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
  // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
  // location [1, 1] in the grid, where [0, 0] is the top left corner.
  float2 samplePos = uv * texSize;
  float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

  // Compute the fractional offset from our starting texel to our original sample location, which we'll
  // feed into the Catmull-Rom spline function to get our filter weights.
  float2 f = samplePos - texPos1;

  // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
  // These equations are pre-expanded based on our knowledge of where the texels will be located,
  // which lets us avoid having to evaluate a piece-wise function.
  float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
  float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
  float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
  float2 w3 = f * f * (-0.5f + 0.5f * f);

  // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
  // simultaneously evaluate the middle 2 samples from the 4x4 grid.
  float2 w12 = w1 + w2;
  float2 offset12 = w2 / (w1 + w2);

  // Compute the final UV coordinates we'll use for sampling the texture
  float2 texPos0 = texPos1 - 1.0F;
  float2 texPos3 = texPos1 + 2.0F;
  float2 texPos12 = texPos1 + offset12;

  texPos0 *= texelSize;
  texPos3 *= texelSize;
  texPos12 *= texelSize;

  float3 result = 0.0f;
  result += tex.SampleLevelZero(float2(texPos0.x, texPos0.y)) * w0.x * w0.y;
  result += tex.SampleLevelZero(float2(texPos12.x, texPos0.y)) * w12.x * w0.y;
  result += tex.SampleLevelZero(float2(texPos3.x, texPos0.y)) * w3.x * w0.y;

  result += tex.SampleLevelZero(float2(texPos0.x, texPos12.y)) * w0.x * w12.y;
  result += tex.SampleLevelZero(float2(texPos12.x, texPos12.y)) * w12.x * w12.y;
  result += tex.SampleLevelZero(float2(texPos3.x, texPos12.y)) * w3.x * w12.y;

  result += tex.SampleLevelZero(float2(texPos0.x, texPos3.y)) * w0.x * w3.y;
  result += tex.SampleLevelZero(float2(texPos12.x, texPos3.y)) * w12.x * w3.y;
  result += tex.SampleLevelZero(float2(texPos3.x, texPos3.y)) * w3.x * w3.y;

  return result;
}

[shader("fragment")]
float3 fragmentMain(float2 texCoords: TEXCOORDS) {
  float3 neighbourMin = float3(1e6);
  float3 neighbourMax = float3(-1e6);
  float3 neighbourSum = 0.0;
  float3 neighbourSumSq = 0.0;
  float neighbourCount = 0.0;

  float3 currentColor;

  float2 texSize;
  IncomingLight.GetDimensions(texSize.x, texSize.y);
  float2 texelSize = 1.0 / texSize;
  float2 halfTexelSize = texelSize * 0.5;

  [[unroll]]
  for (int x = -1; x <= 1; x++) {
    [[unroll]]
    for (int y = -1; y <= 1; y++) {
      float2 sampleCoords = texCoords + float2(x, y) * texelSize;

      if (sampleCoords.x < halfTexelSize.x ||
          sampleCoords.x > 1.0 - halfTexelSize.x ||
          sampleCoords.y < halfTexelSize.y ||
          sampleCoords.y > 1.0 - halfTexelSize.y) {
        continue;
      }

      float3 sampleColor = IncomingLight.SampleLevelZero(sampleCoords);
      neighbourMin = min(neighbourMin, sampleColor);
      neighbourMax = max(neighbourMax, sampleColor);
      neighbourSum += sampleColor;
      neighbourSumSq += sampleColor * sampleColor;
      neighbourCount += 1.0;

      if (x == 0 && y == 0) {
        currentColor = sampleColor;
      }
    }
  }

  float2 motion = UnpackMotionVector(MotionTexture.SampleLevelZero(texCoords));
  float2 historyTexCoords = texCoords - motion;

  bool historyOutOfBounds = historyTexCoords.x < halfTexelSize.x ||
                            historyTexCoords.x > 1.0 - halfTexelSize.x ||
                            historyTexCoords.y < halfTexelSize.y ||
                            historyTexCoords.y > 1.0 - halfTexelSize.y;

  if (historyOutOfBounds) {
    return currentColor;
  }

  float3 historyColor = SampleTextureCatmullRom(
      PreviousIncomingLight, historyTexCoords, texSize, texelSize);

  float3 neighbourMean = neighbourSum / max(neighbourCount, 1.0);
  float3 neighbourVariance =
      max(float3(0.0), neighbourSumSq / max(neighbourCount, 1.0) -
                           neighbourMean * neighbourMean);
  float3 neighbourSigma = sqrt(neighbourVariance);

  const float kVarianceGamma = 1.25;
  float3 varianceMin = neighbourMean - kVarianceGamma * neighbourSigma;
  float3 varianceMax = neighbourMean + kVarianceGamma * neighbourSigma;

  float3 clampedHistoryColor = clamp(historyColor, neighbourMin, neighbourMax);
  clampedHistoryColor = clamp(clampedHistoryColor, varianceMin, varianceMax);

  float motionPixels = length(motion * texSize);
  float motionFactor = saturate(motionPixels * 0.25);

  // Use less history as motion increases to reduce ghosting and background blur.
  const float kStaticHistoryWeight = 0.92;
  const float kDynamicHistoryWeight = 0.55;
  float previousWeight =
      lerp(kStaticHistoryWeight, kDynamicHistoryWeight, motionFactor);

  float sigmaLuma = dot(neighbourSigma, float3(0.299, 0.587, 0.114));
  float currentLuma = dot(currentColor, float3(0.299, 0.587, 0.114));
  float historyLuma = dot(clampedHistoryColor, float3(0.299, 0.587, 0.114));
  float lumaDelta = abs(currentLuma - historyLuma);

  const float kLumaRejectionScale = 4.0;
  float lumaRejection =
      saturate(lumaDelta / max(sigmaLuma * kLumaRejectionScale, 1e-4));
  previousWeight *= 1.0 - lumaRejection;

  float currentWeight = 1.0 - previousWeight;

  float3 finalColor =
      currentColor * currentWeight + clampedHistoryColor * previousWeight;

  // Recover some local detail without destabilizing static regions.
  // float sharpenAmount = lerp(0.06, 0.0, motionFactor);
  // finalColor += (currentColor - neighbourMean) * sharpenAmount;

  return finalColor;
}
