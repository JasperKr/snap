#include "camera.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Scene/cameraMatrices.hpp"
#include "Scene/displayName.hpp"
#include "Scene/environment.hpp"
#include "Scene/frustum.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <flecs.h>
#include <imgui.h>
#include <lauxlib.h>
#include <lua.h>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine {

const auto CameraBufferFormat = Graphics::BufferFormat({
    {"ViewMatrix", "floatmat4"},
    {"InverseViewMatrix", "floatmat4"},
    {"ProjectionMatrix", "floatmat4"},
    {"InverseProjectionMatrix", "floatmat4"},
    {"ViewProjectionMatrix", "floatmat4"},
    {"InverseViewProjectionMatrix", "floatmat4"},
    {"RotationProjectionMatrix", "floatmat4"},
    {"InverseRotationProjectionMatrix", "floatmat4"},
    {"Position", "floatvec3"},
    {"Near", "float"},
    {"Far", "float"},
    {"NearMulFar", "float"},
    {"FarMinusNear", "float"},
    {"HistoryInvalidated", "uint32"},
    {"Jitter", "floatvec2"},
    {"ProjectionType", "uint32"},
    {"ShadowCascadeCount", "uint32"},
});

struct CameraBufferStruct {
  Math::Matrix4x4 ViewMatrix;
  Math::Matrix4x4 InverseViewMatrix;
  Math::Matrix4x4 ProjectionMatrix;
  Math::Matrix4x4 InverseProjectionMatrix;
  Math::Matrix4x4 ViewProjectionMatrix;
  Math::Matrix4x4 InverseViewProjectionMatrix;
  Math::Matrix4x4 RotationProjectionMatrix;
  Math::Matrix4x4 InverseRotationProjectionMatrix;
  Math::Vec3 Position;
  float Near{};
  float Far{};
  float NearMulFar{};
  float FarMinusNear{};
  uint32_t HistoryInvalidated{};
  Math::Vec2 Jitter;
  uint32_t ProjectionType{};
  uint32_t ShadowCascadeCount{};
};

static constexpr std::array<Math::Vec2, 4> JitterPattern = {
    Math::Vec2{2.5F, 0.5F},
    Math::Vec2{0.5F, 1.5F},
    Math::Vec2{3.5F, 2.5F},
    Math::Vec2{1.5F, 3.5F},
};

void Camera::RegisterCameraSystems(Scene &scene) {
  auto cameraProjection =
      scene.world.system<Camera, CameraMatrices>()
          .kind(flecs::PostUpdate)
          .each([](flecs::entity entity, Camera &camera,
                   CameraMatrices &matrices) -> void {
            camera.Jitter = JitterPattern.at(
                Graphics::GetCurrentGraphicsContext()->currentFrame %
                JitterPattern.size());
            camera.Jitter /= 4.0F; // NOLINT
            camera.Jitter -= 0.5F; // NOLINT

            camera.Jitter /= Math::Vec2(camera.Dimensions);
            matrices.Jitter = camera.Jitter;

            if (camera.projectionDirty) {
              matrices.ProjectionMatrix = Math::Matrix4x4::Perspective(
                  camera.VerticalFOVRad, camera.AspectRatio, camera.NearPlane,
                  camera.FarPlane);
              matrices.InverseProjectionMatrix =
                  matrices.ProjectionMatrix.InverseTranspose();

              camera.projectionDirty = false;
            }
          });

  auto cameraTransform =
      scene.world.system<CameraMatrices, Transform>()
          .kind(flecs::PostUpdate)
          .each([](flecs::entity entity, CameraMatrices &matrices,
                   Transform &transform) -> void {
            const auto &worldMatrix = transform.GetWorldMatrix();

            matrices.ViewMatrix = worldMatrix.InverseTranspose();
            matrices.InverseViewMatrix = worldMatrix;
            matrices.RotationMatrix = matrices.ViewMatrix.AsMatrix3x3();
            matrices.InverseRotationMatrix =
                matrices.RotationMatrix.Transpose();
            matrices.Update();
          });

  auto cameraFrustum =
      scene.world.system<CameraMatrices, Frustum>()
          .kind(flecs::PostUpdate)
          .each([](flecs::entity entity, CameraMatrices &matrices,
                   Frustum &frustum) -> void {
            frustum =
                Frustum::FromMatrices(matrices.ViewProjectionMatrix,
                                      matrices.InverseViewProjectionMatrix);
          });

  cameraFrustum.depends_on(cameraTransform);
  cameraTransform.depends_on(cameraProjection);
}

auto Camera::Create(const Graphics::GraphicsContext &context,
                    float verticalFOVDeg, Math::Uvec2 Dimensions, float near,
                    float far) -> Result<Camera> {
  Camera camera;
  camera.verticalFOVDeg = verticalFOVDeg;
  camera.VerticalFOVRad = Math::DegToRad(verticalFOVDeg);
  camera.Dimensions = Dimensions;
  camera.AspectRatio =
      static_cast<float>(Dimensions.x) / static_cast<float>(Dimensions.y);
  camera.NearPlane = near;
  camera.FarPlane = far;

  Graphics::StructuredBufferCreationInfo info{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = static_cast<uint32_t>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) |
                    static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                    static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
      .debugName = "Camera Buffer",
  };

  auto bufferResult =
      Graphics::StructuredBuffer::Create(context, CameraBufferFormat, 2, info);

  if (Error::IsError(bufferResult)) {
    return bufferResult.error();
  }

  camera.CameraBuffer = bufferResult.value();

  camera.ConfigureRendertargets();
  return camera;
}

auto Camera::ApplyPostProcessing(const Graphics::GraphicsContext &context)
    -> Error {
  auto shader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::PostProcessing));
  Graphics::RenderState::SetShader(shader);

  OwnedTextures.PostProcessed =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, Rendertargets.PostProcessed));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                   .texture = OwnedTextures.PostProcessed,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  static auto incomingLightKey = Graphics::ResourceKey{"IncomingLight"};

  [[likely]]
  if (OwnedTextures.BloomFinal.isValid()) {
    CHECK_ERR(shader->Send(incomingLightKey, OwnedTextures.BloomFinal));
  } else {
    // Bloom might be skipped in the case of a really tiny viewport
    CHECK_ERR(shader->Send(incomingLightKey, OwnedTextures.TAA));
  }

  auto temperatureKey = Graphics::ResourceKey{"Temperature"};
  auto tintKey = Graphics::ResourceKey{"Tint"};
  auto applyAGXKey = Graphics::ResourceKey{"ApplyAGX"};
  auto contrastKey = Graphics::ResourceKey{"Contrast"};
  auto saturationKey = Graphics::ResourceKey{"Saturation"};
  auto vignetteKey = Graphics::ResourceKey{"Vignette"};
  auto exposureKey = Graphics::ResourceKey{"Exposure"};

  auto postProcessingConfig = GetPostProcessingConfig();
  CHECK_ERR(Graphics::UniformWriter::Send(shader, temperatureKey,
                                          postProcessingConfig.Temperature));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, tintKey,
                                          postProcessingConfig.Tint));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, applyAGXKey,
                                          postProcessingConfig.ApplyAGX));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, contrastKey,
                                          postProcessingConfig.Contrast));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, saturationKey,
                                          postProcessingConfig.Saturation));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, vignetteKey,
                                          postProcessingConfig.Vignette));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, exposureKey,
                                          postProcessingConfig.Exposure));

  CHECK_ERR(Renderer::DrawFullScreen(context));

  CHECK_ERR(Renderer::GlobalRenderTargetManager.ReleaseRendertarget(
      OwnedTextures.BloomFinal));

  return {};
}

auto Camera::RenderSkybox(const Graphics::GraphicsContext &context,
                          const Environment &environment) -> Error {
  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {{
                    .texture = OwnedTextures.DirectLighting,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                },
                {
                    .blendMode = Graphics::BlendmodeNone,
                    .texture = OwnedTextures.Motion,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                }}));

  auto shader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::Skybox));

  static auto depthBufferKey = Graphics::ResourceKey{"DepthTexture"};
  CHECK_ERR(shader->Send(depthBufferKey, OwnedTextures.Depth));
  CHECK_ERR(shader->Send(cameraBufferKey, CameraBuffer));

  static auto skyboxKey = Graphics::ResourceKey{"SkyboxTexture"};
  CHECK_ERR(shader->Send(skyboxKey, environment.SkyboxTexture));

  Graphics::RenderState::SetShader(shader);
  CHECK_ERR(Renderer::DrawFullScreen(context));

  return {};
}

auto Camera::FillSkybox(const Graphics::GraphicsContext &context,
                        const Environment &environment) -> Error {
  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {{
                   .texture = OwnedTextures.DirectLighting,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  auto shader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::FillSkybox));

  CHECK_ERR(shader->Send(cameraBufferKey, CameraBuffer));

  static auto skyboxKey = Graphics::ResourceKey{"SkyboxTexture"};
  CHECK_ERR(shader->Send(skyboxKey, environment.SkyboxTexture));

  Graphics::RenderState::SetShader(shader);
  CHECK_ERR(Renderer::DrawFullScreen(context));

  return {};
}

auto Camera::UpdateClosestLightProbes(int max, std::vector<uint8_t> &data,
                                      const DrawData &drawData, Scene *scene)
    -> void {

  if (max <= 0) {
    return;
  }

  struct ProbeData {
    Renderer::LightProbe probe;
    Transform transform;
  };

  auto context = *Graphics::GetCurrentGraphicsContext();

  std::vector<std::pair<float, ProbeData>> probes;

  scene->world.each([&](flecs::entity entity, const Transform &transform,
                        const Renderer::LightProbe &lightProbe) -> void {
    auto distanceSqr = (transform.GetPosition() -
                        drawData.Transform.GetWorldMatrix().GetTranslation())
                           .LengthSqr();
    probes.emplace_back(distanceSqr,
                        ProbeData{.probe = lightProbe, .transform = transform});
  });

  std::ranges::sort(probes, [](const auto &first, const auto &second) -> bool {
    return first.first < second.first;
  });

  for (int i = 0; i < std::min(max, static_cast<int>(probes.size())); ++i) {
    const auto &probeData = probes[i].second;
    probeData.probe.WriteToBuffer(
        data, i * Renderer::LightProbeBufferFormat.GetStride(),
        probeData.transform);
  }

  VisibleProbeCount = std::min(max, static_cast<int>(probes.size()));
}

auto Camera::ApplyLightProbes(const Graphics::GraphicsContext &context,
                              const DrawData &drawData, Scene *scene) -> Error {

  Graphics::PushDebugMarker("Apply Light Probes");

  constexpr int MaxLightProbes = 64;

  auto probeData = std::vector<uint8_t>(
      MaxLightProbes * Renderer::LightProbeBufferFormat.GetStride());

  UpdateClosestLightProbes(MaxLightProbes, probeData, drawData, scene);

  CHECK_ERR(
      Renderer::RendererInstance.GetLightProbeBuffer()->GetBuffer()->SetData(
          context, probeData));

  auto &textures = GetOwnedTextures();
  const auto &rendertargets = GetRendertargets();

  textures.Irradiance =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Irradiance));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {{
                    .blendMode = Graphics::BlendmodeAdditive,
                    .texture = textures.DirectLighting,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                },
                {
                    .blendMode = Graphics::BlendmodeNone,
                    .texture = textures.Irradiance,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                }}));

  auto shader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::ApplyEnvironmentMap));
  Graphics::RenderState::SetShader(shader);
  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);

  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};
  CHECK_ERR(shader->Send(cameraBufferKey, CameraBuffer));

  CHECK_ERR(shader->Send(
      {"IrradianceTextures"},
      Renderer::RendererInstance.GetPrefilterManager().GetIrradianceMaps()));
  CHECK_ERR(shader->Send(
      {"SpecularTextures"},
      Renderer::RendererInstance.GetPrefilterManager().GetRadianceMaps()));

  CHECK_ERR(shader->Send(
      {"Probes"},
      Renderer::RendererInstance.GetLightProbeBuffer()->GetBuffer()));

  static auto lightProbeCountKey = Graphics::ResourceKey{"LightProbeCount"};
  CHECK_ERR(Graphics::UniformWriter::Send(
      shader, lightProbeCountKey, static_cast<uint32_t>(VisibleProbeCount)));

  CHECK_ERR(shader->Send({"AlbedoTexture"}, textures.Albedo));
  CHECK_ERR(shader->Send({"NormalTexture"}, textures.Normal));
  CHECK_ERR(shader->Send({"MaterialTexture"}, textures.Material));
  CHECK_ERR(shader->Send({"DepthTexture"}, textures.Depth));
  CHECK_ERR(
      shader->Send({"AmbientOcclusionTexture"}, textures.AmbientOcclusion));
  CHECK_ERR(Graphics::UniformWriter::Send(shader, {"SpecularEnabled"}, 1));

  CHECK_ERR(Renderer::DrawFullScreen(context));

  Graphics::PopDebugMarker();

  return {};
}

// NOLINTNEXTLINE
auto Camera::Render(const Graphics::GraphicsContext &context,
                    const DrawData &drawData, Scene *scene) -> Error {

  CHECK_ERR(Renderer::GlobalRenderTargetManager.ReleaseRendertargets({
      OwnedTextures.DirectLighting,
      OwnedTextures.PreviousDepth,
      OwnedTextures.Normal,
      OwnedTextures.Albedo,
      OwnedTextures.Material,
      OwnedTextures.Emissive,
      OwnedTextures.Motion,
      OwnedTextures.Irradiance,
  }));

  OwnedTextures.PreviousDepth = OwnedTextures.Depth;

  if (!OwnedTextures.PreviousDepth.isValid()) {
    OwnedTextures.PreviousDepth =
        CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
            context, Rendertargets.Depth));
  }

  auto frustum = drawData.Matrices.GetFrustum();

  Graphics::PushDebugMarker("Draw Models");

  CHECK_ERR(scene->DrawModels(*this, frustum, context));

  Graphics::PopDebugMarker();

  OwnedTextures.IncomingLight =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, Rendertargets.IncomingLight));

  const auto &entity = scene->currentEnvironment;
  if (entity.is_valid()) {
    const auto *environment = entity.try_get<Environment>();

    if (environment != nullptr) {
      Graphics::PushDebugMarker("Render Skybox");

      CHECK_ERR(RenderSkybox(context, *environment));

      Graphics::PopDebugMarker();
    }
  }

  CHECK_ERR(ApplyLightProbes(context, drawData, scene));
  Graphics::PushDebugMarker("Write Lighting to Incoming Light");

  Graphics::RenderState::SetShader({});
  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                   .blendMode = Graphics::BlendmodeNone,
                   .texture = OwnedTextures.IncomingLight,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  CHECK_ERR(Graphics::Draw(context, *OwnedTextures.DirectLighting));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                   .blendMode = Graphics::BlendmodeAdditive,
                   .texture = OwnedTextures.IncomingLight,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
               }}));

  CHECK_ERR(Graphics::Draw(context, *OwnedTextures.Irradiance));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {Graphics::RenderState::RenderTarget{
                    .blendMode = Graphics::DefaultBlendMode,
                    .texture = OwnedTextures.IncomingLight,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                },
                Graphics::RenderState::RenderTarget{
                    .blendMode = Graphics::DefaultBlendMode,
                    .texture = OwnedTextures.Depth,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                }}));
  Graphics::PopDebugMarker();

  Graphics::PushDebugMarker("Draw Lines");
  CHECK_ERR(Renderer::RendererInstance.GetLineDrawer().Render(context, *this));
  CHECK_ERR(
      Renderer::RendererInstance.GetPrimitiveDrawer().Render(context, *this));
  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Temporal Anti-Aliasing");

  auto taaShader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::TAA));

  OwnedTextures.TAA =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, Rendertargets.TemporalAntiAliasing));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {{
                   .blendMode = Graphics::BlendmodeNone,
                   .texture = OwnedTextures.TAA,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  if (!OwnedTextures.PreviousTAA.isValid()) {
    OwnedTextures.PreviousTAA =
        CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
            context, Rendertargets.TemporalAntiAliasing));
  }

  Graphics::RenderState::SetShader(taaShader);
  CHECK_ERR(taaShader->Send({"IncomingLight"}, OwnedTextures.IncomingLight));
  CHECK_ERR(
      taaShader->Send({"PreviousIncomingLight"}, OwnedTextures.PreviousTAA));
  CHECK_ERR(taaShader->Send({"MotionTexture"}, OwnedTextures.Motion));

  CHECK_ERR(Renderer::DrawFullScreen(context));

  CHECK_ERR(Renderer::GlobalRenderTargetManager.ReleaseRendertarget(
      OwnedTextures.PreviousTAA));

  OwnedTextures.PreviousTAA = OwnedTextures.TAA;

  Graphics::PopDebugMarker();

  if (settings.DoPostProcessing) {
    Graphics::PushDebugMarker("Apply Bloom");
    CHECK_ERR(Renderer::RendererInstance.GetBloomManager().ApplyBloom(context,
                                                                      *this));
    Graphics::PopDebugMarker();

    Graphics::PushDebugMarker("Apply Post Processing");
    CHECK_ERR(ApplyPostProcessing(context));
    Graphics::PopDebugMarker();
  }

  return {};
}

auto Camera::RenderSkyboxOnly(const Graphics::GraphicsContext &context,
                              const Environment &environment) -> Error {
  OwnedTextures.DirectLighting =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, Rendertargets.DirectLighting));

  CHECK_ERR(FillSkybox(context, environment));

  return {};
}

auto Camera::ConfigureRendertargets() -> void {
  // Default clamp = edge
  // Default border color = white

  Rendertargets.Depth = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_D32_SFLOAT,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.Normal = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.Albedo = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.Material = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.Emissive = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.Motion = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_R16G16_SNORM,
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.IncomingLight = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.DirectLighting = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.Irradiance = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.PostProcessed = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.BloomDownsampleChain = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .requiresMipmaps = true,
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.ShadowVisibility = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .arrayLayers = 16, // NOLINT
      .format = VK_FORMAT_R8_UNORM,
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.ShadowHitFlags = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_R32_UINT,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.AmbientOcclusion = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_R8G8_UNORM,
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.AmbientOcclusionSamples = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_R8_UNORM,
      .minFilter = VK_FILTER_NEAREST,
      .magFilter = VK_FILTER_NEAREST,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };

  Rendertargets.TemporalAntiAliasing = Renderer::RendertargetDescriptor{
      .size = Dimensions,
      .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
      .minFilter = VK_FILTER_LINEAR,
      .magFilter = VK_FILTER_LINEAR,
      .mipFilter = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
  };
}

auto Camera::WriteToBuffer(const CameraMatrices &cameraMatrices,
                           const Transform &transform) const -> Error {
  auto context = *Graphics::GetCurrentGraphicsContext();

  assert(sizeof(CameraBufferStruct) == CameraBuffer->GetStride());

  CHECK_ERR(CameraBuffer->GetBuffer()->CopyTo(
      context, *CameraBuffer->GetBuffer(), 0, sizeof(CameraBufferStruct),
      sizeof(CameraBufferStruct)));

  auto data = CameraBufferStruct{
      .ViewMatrix = cameraMatrices.ViewMatrix,
      .InverseViewMatrix = cameraMatrices.InverseViewMatrix,
      .ProjectionMatrix = cameraMatrices.ProjectionMatrix,
      .InverseProjectionMatrix = cameraMatrices.InverseProjectionMatrix,
      .ViewProjectionMatrix = cameraMatrices.ViewProjectionMatrix,
      .InverseViewProjectionMatrix = cameraMatrices.InverseViewProjectionMatrix,
      .RotationProjectionMatrix = cameraMatrices.RotationProjectionMatrix,
      .InverseRotationProjectionMatrix =
          cameraMatrices.InverseRotationProjectionMatrix,
      .Position = transform.GetPosition(),
      .Near = NearPlane,
      .Far = FarPlane,
      .NearMulFar = NearPlane * FarPlane,
      .FarMinusNear = FarPlane - NearPlane,
      .HistoryInvalidated = 0,
      .Jitter = Jitter,
      .ProjectionType = 0,
      .ShadowCascadeCount = 0,
  };

  const auto *uint8Ptr = reinterpret_cast<const uint8_t *>(&data); // NOLINT
  auto span = std::span<const uint8_t>(uint8Ptr, sizeof(CameraBufferStruct));

  return CameraBuffer->GetBuffer()->SetData(context, span);
}

// NOLINTBEGIN
auto Camera::DrawGUI(flecs::entity entity) -> void {
  const int MinFOV = 1;
  const int MaxFOV = 175;
  const float MinNear = 0.01F;
  const float MaxNear = 10000.0F;

  ImGui::SeparatorText("Camera Settings");
  ImGui::DragFloat("Vertical FOV", &verticalFOVDeg, 1.0F, MinFOV, MaxFOV);
  ImGui::DragFloat("Near Plane", &NearPlane, 1.0F, MinNear, MaxNear);
  ImGui::DragFloat("Far Plane", &FarPlane, 1.0F, MinNear, MaxNear);
  FarPlane = std::max(FarPlane, NearPlane + 1.0F);

  ImGui::SeparatorText("Post Processing Settings");
  auto &settings = GetPostProcessingConfig();
  ImGui::Checkbox("AGX", &settings.ApplyAGX);
  ImGui::DragFloat("Temperature", &settings.Temperature, 0.1F, -100.0F, 100.0F);
  ImGui::DragFloat("Tint", &settings.Tint, 0.1F, -100.0F, 100.0F);
  ImGui::DragFloat("Contrast", &settings.Contrast, 0.01F, 0.0F, 10.0F);
  ImGui::DragFloat("Saturation", &settings.Saturation, 0.01F, 0.0F, 10.0F);
  ImGui::DragFloat("Vignette", &settings.Vignette, 0.01F, 0.0F, 10.0F);
  ImGui::DragFloat("Exposure", &settings.Exposure, 0.01F, 0.0F, 10.0F);
}
// NOLINTEND

// MARK: Lua Camera

// scene:newCamera(name, verticalFOV, width, height, near, far)
auto LuaCamera::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));

  auto context = *Graphics::GetCurrentGraphicsContext();

  const auto *name = luaL_optstring(state, 2, "Camera");
  const auto fov = luaL_optscalar(state, 3, 60.0);
  const auto width = luaL_optint(state, 4, Window::GetWidth(context.sdlWindow));
  const auto height =
      luaL_optint(state, 5, Window::GetHeight(context.sdlWindow));
  const auto near = luaL_optscalar(state, 6, 0.1F);
  const auto far = luaL_optscalar(state, 7, 1000.0F);

  auto cameraResult = LUA_CK_RES(Camera::Create(
      context, fov,
      {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, near,
      far));

  auto entity = scene->world.entity();
  entity.set<Camera>(cameraResult);
  entity.add<CameraMatrices>();
  entity.add<Frustum>();
  entity.add<Transform>();
  entity.add<Userdata>();
  entity.set<DisplayName>({luaL_optstring(state, 2, "Camera")});

  auto camera = Ref<LuaCamera>::Make(entity);

  ::LuaWrap::PushObject(state, LuaCamera::GetType(), camera.get());
  return 1;
}

auto LuaCamera::SetAspectRatio(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  camera->SetAspectRatio(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::SetVerticalFOV(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  camera->SetVerticalFOV(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::SetNearPlane(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  camera->SetNearPlane(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::SetFarPlane(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  camera->SetFarPlane(luaL_checkscalar(state, 2));

  return 0;
}

auto LuaCamera::GetAspectRatio(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  lua_pushnumber(state, camera->GetAspectRatio());
  return 1;
}

auto LuaCamera::GetVerticalFOV(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  lua_pushnumber(state, camera->GetVerticalFOV());
  return 1;
}

auto LuaCamera::GetNearPlane(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  lua_pushnumber(state, camera->GetNearPlane());
  return 1;
}

auto LuaCamera::GetFarPlane(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  lua_pushnumber(state, camera->GetFarPlane());
  return 1;
}

auto LuaCamera::GetBuffer(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  ::LuaWrap::PushObject(state, Graphics::StructuredBuffer::GetType(),
                        camera->GetBuffer().get());
  return 1;
}

auto LuaCamera::Render(lua_State *state) -> int {
  ZoneScoped;

  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 2));
  auto context = *Graphics::GetCurrentGraphicsContext();

  DrawData drawData{
      .Transform = *obj->entity.try_get<Transform>(),
      .Matrices = *obj->entity.try_get<CameraMatrices>(),
  };

  LUA_CK_ERR(camera->Render(context, drawData, scene.get()));

  return 0;
}

auto LuaCamera::GetRendertarget(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  auto context = *Graphics::GetCurrentGraphicsContext();

  const auto *name = luaL_checkstring(state, 2);

  const std::unordered_map<std::string_view,
                           Ref<Graphics::Texture> Camera::AllocatedTextures::*>
      allocatedTextureMap = {
          {"Depth", &Camera::AllocatedTextures::Depth},
          {"Normal", &Camera::AllocatedTextures::Normal},
          {"Albedo", &Camera::AllocatedTextures::Albedo},
          {"Material", &Camera::AllocatedTextures::Material},
          {"Emissive", &Camera::AllocatedTextures::Emissive},
          {"Motion", &Camera::AllocatedTextures::Motion},
          {"IncomingLight", &Camera::AllocatedTextures::IncomingLight},
          {"TAA", &Camera::AllocatedTextures::TAA},
          {"PreviousTAA", &Camera::AllocatedTextures::PreviousTAA},
          {"DirectLighting", &Camera::AllocatedTextures::DirectLighting},
          {"Irradiance", &Camera::AllocatedTextures::Irradiance},
          {"PostProcessed", &Camera::AllocatedTextures::PostProcessed},
      };

  auto iter = allocatedTextureMap.find(name);
  if (iter == allocatedTextureMap.end()) {
    return luaL_error(state, "Invalid rendertarget name: %s", name);
  }

  auto &texture = camera->OwnedTextures.*(iter->second);

  ::LuaWrap::PushObject(state, Graphics::Texture::GetType(), texture.get());
  return 1;
}

auto LuaCamera::GetDimensions(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  const auto &size = camera->GetDimensions();

  lua_pushinteger(state, size.x);
  lua_pushinteger(state, size.y);
  return 2;
}

auto LuaCamera::SetDimensions(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  const auto width = luaL_checkinteger(state, 2);
  const auto height = luaL_checkinteger(state, 3);

  camera->SetDimensions(
      {static_cast<uint32_t>(width), static_cast<uint32_t>(height)});

  return 0;
}

auto LuaCamera::GetWidth(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  const auto &size = camera->GetDimensions();

  lua_pushinteger(state, size.x);
  return 1;
}

auto LuaCamera::GetHeight(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  const auto &size = camera->GetDimensions();

  lua_pushinteger(state, size.y);
  return 1;
}

auto LuaCamera::SetWidth(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  const auto width = luaL_checkinteger(state, 2);
  const auto &size = camera->GetDimensions();

  camera->SetDimensions({static_cast<uint32_t>(width), size.y});

  return 0;
}

auto LuaCamera::SetHeight(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  const auto height = luaL_checkinteger(state, 2);
  const auto &size = camera->GetDimensions();

  camera->SetDimensions({size.x, static_cast<uint32_t>(height)});

  return 0;
}

const std::unordered_map<std::string_view,
                         bool Camera::PersistentTextureSettings::*>
    persistentTextureMap = {
        {"Depth", &Camera::PersistentTextureSettings::Depth},
        {"Normal", &Camera::PersistentTextureSettings::Normal},
        {"Albedo", &Camera::PersistentTextureSettings::Albedo},
        {"Material", &Camera::PersistentTextureSettings::Material},
        {"Emissive", &Camera::PersistentTextureSettings::Emissive},
        {"Motion", &Camera::PersistentTextureSettings::Motion},
        {"IncomingLight", &Camera::PersistentTextureSettings::IncomingLight},
        {"TAA", &Camera::PersistentTextureSettings::TAA},
        {"PreviousTAA", &Camera::PersistentTextureSettings::PreviousTAA},
        {"DirectLighting", &Camera::PersistentTextureSettings::DirectLighting},
        {"Irradiance", &Camera::PersistentTextureSettings::Irradiance},
        {"PostProcessed", &Camera::PersistentTextureSettings::PostProcessed},
};

auto LuaCamera::GetPersistentTextureSettings(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  const auto *camera = LUA_CK_NULL(obj->entity.try_get<Camera>());

  const auto &settings = camera->GetPersistentTextureSettings();

  lua_newtable(state);

  for (const auto &[name, ptr] : persistentTextureMap) {
    lua_pushstring(state, name.data());
    lua_pushboolean(state, static_cast<int>(settings.*ptr));
    lua_settable(state, -3);
  }

  return 1;
}

auto LuaCamera::SetPersistentTextureSettings(lua_State *state) -> int {
  auto obj = LUA_CK_NULL(::LuaWrap::ObjectFromLua<LuaCamera>(state, 1));
  auto *camera = LUA_CK_NULL(obj->entity.try_get_mut<Camera>());

  Camera::PersistentTextureSettings newSettings;

  luaL_checktype(state, 2, LUA_TTABLE);

  for (const auto &[name, ptr] : persistentTextureMap) {
    lua_pushstring(state, name.data());
    lua_gettable(state, 2);
    if (lua_isboolean(state, -1)) {
      newSettings.*ptr = static_cast<bool>(lua_toboolean(state, -1));
    }
    lua_pop(state, 1);
  }

  camera->SetPersistentTextureSettings(newSettings);

  return 0;
}

auto GetLuaCameraClass() -> ::LuaWrap::LuaClass {
  const ::LuaWrap::LuaClass LuaCameraClass = {
      .Name = "Camera",
      .Type = LuaCamera::GetType(),
      .Methods =
          {
              {"getAspectRatio", LuaCamera::GetAspectRatio},
              {"setAspectRatio", LuaCamera::SetAspectRatio},
              {"getVerticalFOV", LuaCamera::GetVerticalFOV},
              {"setVerticalFOV", LuaCamera::SetVerticalFOV},
              {"getNearPlane", LuaCamera::GetNearPlane},
              {"setNearPlane", LuaCamera::SetNearPlane},
              {"getFarPlane", LuaCamera::GetFarPlane},
              {"setFarPlane", LuaCamera::SetFarPlane},
              {"getBuffer", LuaCamera::GetBuffer},
              {"render", LuaCamera::Render},
              {"getRendertarget", LuaCamera::GetRendertarget},
              {"getDimensions", LuaCamera::GetDimensions},
              {"setDimensions", LuaCamera::SetDimensions},
              {"getWidth", LuaCamera::GetWidth},
              {"setWidth", LuaCamera::SetWidth},
              {"getHeight", LuaCamera::GetHeight},
              {"setHeight", LuaCamera::SetHeight},
              {"getPersistentTextureSettings",
               LuaCamera::GetPersistentTextureSettings},
              {"setPersistentTextureSettings",
               LuaCamera::SetPersistentTextureSettings},
          },
      .Components = {
          TransformComponent,
          CameraMatricesComponent,
          FrustumComponent,
          DisplayNameComponent,
          UserdataComponent,
      }};

  return LuaCameraClass;
}

} // namespace Engine