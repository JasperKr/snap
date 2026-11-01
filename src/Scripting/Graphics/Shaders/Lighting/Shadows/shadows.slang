import raytracing;
import camera;
import utils;
import constants;
import random;

#ifdef SHADER_LIGHT_TYPE_DIRECTIONAL
import directionalLight;
#elif defined(SHADER_LIGHT_TYPE_POINT)
import pointLight;
#elif defined(SHADER_LIGHT_TYPE_SPOT)
import spotLight;
#elif defined(SHADER_LIGHT_TYPE_SPHERE)
import sphereLight;
#elif defined(SHADER_LIGHT_TYPE_RECTANGLE)
import rectangleLight;
#else
#error "No light type defined for shadow sampling shader"
#endif

uniform Sampler2D.Handle DepthTexture;
uniform Sampler2D.Handle NormalTexture;
uniform RWTexture2D<uint>.Handle OutputTexture;

uniform uint LightIndex;
uniform uint LayerOffset;
uniform uint FrameRandomState;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 id: SV_DispatchThreadID) {
  uint width;
  uint height;
  OutputTexture.GetDimensions(width, height);

  if (id.x >= width || id.y >= height) {
    return;
  }

  id.z += LayerOffset;

  float2 uv = (float2(id.xy) + 0.5) / float2(width, height);

  float3 normal = NormalTexture.SampleLevelZero(uv).xyz;
  normal = normalize(normal * 2.0f - 1.0f);
  float depth = DepthTexture.SampleLevelZero(uv).x;

  if (depth == DepthFarPlane || depth == DepthNearPlane) {
    OutputTexture.Store(id.xy, 1);
    return;
  }

  float3 worldPosition = CameraData[0].GetWorldPosition(uv, depth);
  uint2 screenCoords = uint2(id.xy);
  uint randomState =
      FrameRandomState + screenCoords.x * 1702 + screenCoords.y * 40301;

#ifdef SHADER_LIGHT_TYPE_DIRECTIONAL
  var light = DirectionalLights.Load(LightIndex + id.z);
  bool inShadow =
      light.TraceShadow(worldPosition, normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_POINT)
  var light = PointLights.Load(LightIndex + id.z);
  bool inShadow =
      light.TraceShadow(worldPosition, normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_SPOT)
  var light = SpotLights.Load(LightIndex + id.z);
  bool inShadow =
      light.TraceShadow(worldPosition, normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_SPHERE)
  var light = SphereLights.Load(LightIndex + id.z);
  bool inShadow =
      light.TraceShadow(worldPosition, normal, screenCoords, randomState);
#elif defined(SHADER_LIGHT_TYPE_RECTANGLE)
  var light = RectangleLights.Load(LightIndex + id.z);
  bool inShadow =
      light.TraceShadow(worldPosition, normal, screenCoords, randomState);
#else
#error "Invalid light type";
  bool inShadow = false;
#endif

  uint shadowBit = inShadow ? 1 : 0;

  OutputTexture[id.xy] = shadowBit;
}
