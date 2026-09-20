#include "scene.hpp"
#include "Editor/editor.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/bindings.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Renderer/lightProbe.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/model.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/light.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "Scene/camera.hpp"
#include "Scene/environment.hpp"
#include "Scene/frustum.hpp"
#include "Scene/node.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "material.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <flecs.h>
#include <imgui.h>
#include <lua.hpp>
#include <public/tracy/Tracy.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "../Snap/Modules/Peripherals/keyboard.hpp"

namespace Engine {

auto LuaScene::LoadBinding(lua_State *state) -> int {
  Bindings::LuaBoundStruct<Scene> bindings("Scene");
  bindings.RegisterMember<&Scene::name>("Name");
  bindings.DocumentCustomMethod(Bindings::MethodInfo{
      .name = "DrawUiElement",
      .description = "Draws a UI element for this scene."});
  bindings.Register(state);

  return 0;
}

struct DrawItem {
  flecs::entity geom_entity;
  Geometry geometry;
  const Renderer::Material *material{};
  uint32_t transformIndex{};

  uint64_t primaryKey;
  uint64_t secondaryKey;
  uint64_t tertiaryKey;
};

inline auto CompareDrawItems(const DrawItem &first, const DrawItem &second)
    -> bool {
  if (first.primaryKey != second.primaryKey) {
    return first.primaryKey < second.primaryKey;
  }
  if (first.secondaryKey != second.secondaryKey) {
    return first.secondaryKey < second.secondaryKey;
  }
  return first.tertiaryKey < second.tertiaryKey;
}

inline auto BindMaterial(const Ref<Graphics::Shader> &shader,
                         Graphics::GraphicsContext &ctx,
                         const Renderer::Material *material, uint8_t flags)
    -> Error {
  const auto &defaultMaterial = Renderer::RendererInstance.GetDefaultMaterial();

  auto albedoTexture = material->albedoTexture;
  if (!albedoTexture.isValid()) {
    albedoTexture = defaultMaterial.albedoTexture;
  }

  auto metallicRoughnessTexture = material->metallicRoughnessTexture;
  if (!metallicRoughnessTexture.isValid()) {
    metallicRoughnessTexture = defaultMaterial.metallicRoughnessTexture;
  }

  auto ambientOcclusionTexture = material->ambientOcclusionTexture;
  if (!ambientOcclusionTexture.isValid()) {
    ambientOcclusionTexture = defaultMaterial.ambientOcclusionTexture;
  }

  auto normalTexture = material->normalTexture;
  if (!normalTexture.isValid()) {
    normalTexture = defaultMaterial.normalTexture;
  }

  auto emissiveTexture = material->emissiveTexture;
  if (!emissiveTexture.isValid()) {
    emissiveTexture = defaultMaterial.emissiveTexture;
  }
  auto reflectanceTexture = material->reflectanceTexture;
  if (!reflectanceTexture.isValid()) {
    reflectanceTexture = defaultMaterial.reflectanceTexture;
  }

  // NOLINTBEGIN
  if ((flags & 1U) != 0U) {
    static auto albedoKey = Graphics::ResourceKey{"AlbedoTexture"};
    CHECK_ERR(shader->Send(albedoKey, albedoTexture));
  }

  if ((flags & 2U) != 0U) {
    static auto metallicRoughnessKey =
        Graphics::ResourceKey{"MetallicRoughnessTexture"};
    CHECK_ERR(shader->Send(metallicRoughnessKey, metallicRoughnessTexture));
  }

  // if ((flags & 4U) != 0U) {
  //   static auto ambientOcclusionTextureKey =
  //       Graphics::ResourceKey{"AmbientOcclusionTexture"};

  //   CHECK_ERR(
  //       shader->Send(ambientOcclusionTextureKey, ambientOcclusionTexture));
  // }

  if ((flags & 8U) != 0U) {
    static auto normalTextureKey = Graphics::ResourceKey{"NormalTexture"};
    CHECK_ERR(shader->Send(normalTextureKey, normalTexture));
  }

  if ((flags & 16U) != 0U) {
    static auto emissiveTextureKey = Graphics::ResourceKey{"EmissiveTexture"};
    CHECK_ERR(shader->Send(emissiveTextureKey, emissiveTexture));
  }

  if ((flags & 32U) != 0U) {
    static auto reflectanceTextureKey =
        Graphics::ResourceKey{"ReflectanceTexture"};
    CHECK_ERR(shader->Send(reflectanceTextureKey, reflectanceTexture));
  }
  // NOLINTEND

  return {};
}

struct DrawConfig {
  /*
    albedoTexture
    metallicRoughnessTexture
    ambientOcclusionTexture
    normalTexture
    emissiveTexture
    reflectanceTexture
  */
  uint8_t bindMaterialTextures{};
  bool bindMaterialBuffer = false;
};

inline auto RenderDrawItem(const DrawItem &item,
                           const Ref<Graphics::Shader> &shader,
                           Graphics::GraphicsContext &ctx,
                           const DrawConfig &config) -> Error {
  if (config.bindMaterialTextures != 0) {
    CHECK_ERR(
        BindMaterial(shader, ctx, item.material, config.bindMaterialTextures));
  }

  if (config.bindMaterialBuffer) {
    static auto materialBufferIndexKey =
        Graphics::ResourceKey{"PushConstants", "MaterialBufferIndex"};
    CHECK_ERR(Graphics::UniformWriter::Send(shader, materialBufferIndexKey,
                                            item.material->materialSSBOIndex));
  }

  static auto modelTransformIndexKey =
      Graphics::ResourceKey{"PushConstants", "ModelTransformIndex"};
  CHECK_ERR(Graphics::UniformWriter::Send(shader, modelTransformIndexKey,
                                          item.transformIndex));

  if (item.geometry.mesh.get() == nullptr) {
    return Error::Create("Invalid geometry mesh");
  }

  Graphics::RenderState::SetCullMode(item.material->cullMode);

  CHECK_ERR(Graphics::Draw(ctx, *item.geometry.mesh));

  return {};
}

inline auto AddDrawItem(std::vector<DrawItem> &OpaqueDrawItems,
                        std::vector<DrawItem> &MaskedDrawItems,
                        std::vector<DrawItem> &TransparentDrawItems,
                        flecs::entity entity, const Geometry &geometry,
                        const Frustum &frustum) -> void {
  const Renderer::Material *material = nullptr;
  material = entity.try_get<Renderer::Material>();

  if (material == nullptr) {
    material = &Renderer::RendererInstance.GetNoMaterial();
  }

  const auto &worldBounds = entity.get<WorldBounds>();
  const auto &boundingBox = worldBounds.Bounds;

  if (!frustum.IntersectsAABB(boundingBox)) {
    return; // Skip entities that are outside the camera frustum
  }

  auto drawItem = DrawItem{.geom_entity = entity,
                           .geometry = geometry,
                           .material = material,
                           .primaryKey = material->getID(),
                           .secondaryKey = geometry.mesh->getID(),
                           .tertiaryKey = material->GetSecondarySortKey()};

  switch (material->alphaMode) {
  case Renderer::AlphaMode::Opaque:
    OpaqueDrawItems.emplace_back(drawItem);
    break;
  case Renderer::AlphaMode::Mask:
    MaskedDrawItems.emplace_back(drawItem);
    break;
  case Renderer::AlphaMode::Blend:
    TransparentDrawItems.emplace_back(drawItem);
    break;
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Scene::DrawModels(Camera &camera, Frustum &frustum,
                       const Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;

  // Pre-frame setup
  auto &renderer = Renderer::RendererInstance;

  renderer.NewFrame();

  for (const auto &system : preRender) {
    system.run();
  }
  finalizePreRenderUploads.run();

  // Shader configuration

  auto &ctx = *Graphics::GetCurrentGraphicsContext();
  auto &textures = camera.GetOwnedTextures();
  auto &manager = renderer.GetShaderManager();

  auto depthOpaque =
      CHECK_RES(manager.GetShader(Renderer::ShaderKey::DepthPrepass));
  auto depthMasked =
      CHECK_RES(manager.GetShader(Renderer::ShaderKey::DepthMaskPass));
  auto deferred = CHECK_RES(manager.GetShader(Renderer::ShaderKey::Deferred));
  auto forward =
      CHECK_RES(manager.GetShader(Renderer::ShaderKey::TransparencyForward));

  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};
  auto cameraBuffer = camera.GetBuffer()->GetBuffer();

  CHECK_ERR(Graphics::UniformWriter::Send(depthOpaque, cameraBufferKey,
                                          cameraBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(depthMasked, cameraBufferKey,
                                          cameraBuffer));
  CHECK_ERR(
      Graphics::UniformWriter::Send(deferred, cameraBufferKey, cameraBuffer));
  CHECK_ERR(
      Graphics::UniformWriter::Send(forward, cameraBufferKey, cameraBuffer));

  static auto modelTransformsBufferKey =
      Graphics::ResourceKey{"ModelTransforms"};

  auto modelTransformsBuffer = renderer.GetModelTransformsBuffer()->GetBuffer();
  CHECK_ERR(Graphics::UniformWriter::Send(depthOpaque, modelTransformsBufferKey,
                                          modelTransformsBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(depthMasked, modelTransformsBufferKey,
                                          modelTransformsBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(deferred, modelTransformsBufferKey,
                                          modelTransformsBuffer));
  CHECK_ERR(Graphics::UniformWriter::Send(forward, modelTransformsBufferKey,
                                          modelTransformsBuffer));

  static auto materialBufferKey = Graphics::ResourceKey{"MaterialBuffer"};
  auto materialBuffer = renderer.GetMaterialsBuffer()->GetBuffer();
  CHECK_ERR(depthMasked->Send(materialBufferKey, materialBuffer));
  CHECK_ERR(deferred->Send(materialBufferKey, materialBuffer));
  CHECK_ERR(forward->Send(materialBufferKey, materialBuffer));

  // Draw sorting

  static std::vector<DrawItem> OpaqueDrawItems;
  static std::vector<DrawItem> MaskedDrawItems;
  static std::vector<DrawItem> TransparentDrawItems;

  snap_defer(OpaqueDrawItems.clear());
  snap_defer(MaskedDrawItems.clear());
  snap_defer(TransparentDrawItems.clear());

  world.each<Geometry>(
      [&](flecs::entity entity, const Geometry &geometry) -> void {
        bool hasChildren = false;
        entity.children([&](flecs::entity child) -> void {
          hasChildren = true;

          AddDrawItem(OpaqueDrawItems, MaskedDrawItems, TransparentDrawItems,
                      child, geometry, frustum);
        });

        if (!hasChildren) {
          AddDrawItem(OpaqueDrawItems, MaskedDrawItems, TransparentDrawItems,
                      entity, geometry, frustum);
        }
      });

  std::ranges::sort(OpaqueDrawItems, CompareDrawItems);
  std::ranges::sort(MaskedDrawItems, CompareDrawItems);
  std::ranges::sort(TransparentDrawItems, CompareDrawItems);

  // Prepare model transform buffer

  struct ModelTransformData {
    std::array<float, 16> modelMatrix{};  // NOLINT
    std::array<float, 16> normalMatrix{}; // NOLINT

    constexpr explicit ModelTransformData(Transform transform) {
      std::memcpy(modelMatrix.data(), // NOLINT
                  transform.GetWorldMatrix().floatData(),
                  sizeof(float) * Math::Matrix4x4::Size);

      std::memcpy(normalMatrix.data(), // NOLINT
                  Math::Matrix4x4(transform.GetNormalMatrix()).floatData(),
                  sizeof(float) * Math::Matrix4x4::Size);
    }
  };

  std::vector<ModelTransformData> modelTransforms;
  size_t transformCount = 0UL;
  modelTransforms.reserve(OpaqueDrawItems.size() + MaskedDrawItems.size() +
                          TransparentDrawItems.size());

  for (auto &item : OpaqueDrawItems) {
    item.transformIndex = transformCount++;
    modelTransforms.emplace_back(item.geom_entity.get<Transform>());
  }

  for (auto &item : MaskedDrawItems) {
    item.transformIndex = transformCount++;
    modelTransforms.emplace_back(item.geom_entity.get<Transform>());
  }

  for (auto &item : TransparentDrawItems) {
    item.transformIndex = transformCount++;
    modelTransforms.emplace_back(item.geom_entity.get<Transform>());
  }

  CHECK_ERR(renderer.AssureModelTransformBufferSize(transformCount));

  auto span = std::span<const ModelTransformData>(modelTransforms.data(),
                                                  modelTransforms.size());

  CHECK_ERR(
      renderer.GetModelTransformsBuffer()->GetBuffer()->SetData(ctx, span));

  static auto albedoTextureKey = Graphics::ResourceKey{"AlbedoTexture"};
  static auto normalTextureKey = Graphics::ResourceKey{"NormalTexture"};
  static auto materialTextureKey = Graphics::ResourceKey{"MaterialTexture"};
  static auto emissiveTextureKey = Graphics::ResourceKey{"EmissiveTexture"};
  static auto depthBufferKey = Graphics::ResourceKey{"DepthTexture"};
  static auto previousDepthBufferKey =
      Graphics::ResourceKey{"PreviousDepthTexture"};

  const auto &rendertargets = camera.GetRendertargets();

  // Depth Opaque prepass

  textures.Depth =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Depth));

  Graphics::RenderState::SetDepthMode(true, true, VK_COMPARE_OP_GREATER);
  Graphics::RenderState::SetWindingOrder(VK_FRONT_FACE_COUNTER_CLOCKWISE);
  Graphics::RenderState::SetShader(depthOpaque);

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      ctx, {{
               .clearValue = VkClearValue{0.0F, 0},
               .texture = textures.Depth,
               .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
           }}));

  static auto opaqueConfig = DrawConfig{
      .bindMaterialTextures = 0,
      .bindMaterialBuffer = false,
  };

  Graphics::PushDebugMarker("Depth Opaque Prepass");

  for (const auto &item : OpaqueDrawItems) {
    CHECK_ERR(RenderDrawItem(item, depthOpaque, ctx, opaqueConfig));
  }

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Depth Masked Prepass");

  static auto maskedConfig = DrawConfig{
      .bindMaterialTextures = 1,
      .bindMaterialBuffer = true,
  };
  Graphics::RenderState::SetShader(depthMasked);
  for (const auto &item : MaskedDrawItems) {
    CHECK_ERR(RenderDrawItem(item, depthMasked, ctx, maskedConfig));
  }

  Graphics::PopDebugMarker();

  textures.Albedo =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Albedo));
  textures.Normal =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Normal));
  textures.Material =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Material));
  textures.Emissive =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Emissive));
  textures.Motion =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.Motion));

  Graphics::RenderState::SetDepthMode(true, false, VK_COMPARE_OP_EQUAL);
  Graphics::RenderState::SetShader(deferred);

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      ctx, {{
                .texture = textures.Depth,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Albedo,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Normal,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Material,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Emissive,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            },
            {
                .blendMode = Graphics::BlendmodeNone,
                .texture = textures.Motion,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            }}));

  static auto deferredConfig = DrawConfig{
      .bindMaterialTextures = UINT8_MAX,
      .bindMaterialBuffer = true,
  };

  Graphics::PushDebugMarker("Opaque Materials");

  for (const auto &item : OpaqueDrawItems) {
    CHECK_ERR(RenderDrawItem(item, deferred, ctx, deferredConfig));
  }

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Masked Materials");

  for (const auto &item : MaskedDrawItems) {
    CHECK_ERR(RenderDrawItem(item, deferred, ctx, deferredConfig));
  }

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Lighting");

  if (OpaqueDrawItems.empty() && MaskedDrawItems.empty()) {
    CHECK_ERR(Graphics::RenderState::Clear(
        ctx, {
                 .colors =
                     {
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Albedo
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Normal
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Material
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Emissive
                         {0.0F, 0.0F, 0.0F, 1.0F}, // Motion
                     },
                 .depthClearValue = 0.0F,
                 .clearDepth = true,
             }));
  }

  auto lightingShader =
      CHECK_RES(manager.GetShader(Renderer::ShaderKey::SimpleLighting));
  CHECK_ERR(lightingShader->Send(cameraBufferKey, cameraBuffer));
  CHECK_ERR(lightingShader->Send(albedoTextureKey, textures.Albedo));
  CHECK_ERR(lightingShader->Send(materialTextureKey, textures.Material));
  CHECK_ERR(lightingShader->Send(emissiveTextureKey, textures.Emissive));

  auto sendInfo = [&](const Ref<Graphics::Shader> &shader) -> Error {
    CHECK_ERR(shader->Send(normalTextureKey, textures.Normal));
    CHECK_ERR(shader->Send(depthBufferKey, textures.Depth));

    return {};
  };

  using K = Renderer::ShaderKey;

  auto shadowPoint = CHECK_RES(manager.GetShader(K::Shadows_Point));
  auto shadowSpot = CHECK_RES(manager.GetShader(K::Shadows_Spot));
  auto shadowSphere = CHECK_RES(manager.GetShader(K::Shadows_Sphere));
  auto shadowRectangle = CHECK_RES(manager.GetShader(K::Shadows_Rectangle));
  auto shadowDenoise = CHECK_RES(manager.GetShader(K::Shadows_Denoise));

  CHECK_ERR(sendInfo(lightingShader));
  // CHECK_ERR(sendInfo(shadowPoint));
  // CHECK_ERR(sendInfo(shadowSpot));
  // CHECK_ERR(sendInfo(shadowSphere));
  // CHECK_ERR(sendInfo(shadowRectangle));
  CHECK_ERR(shadowDenoise->Send(depthBufferKey, textures.Depth));

  CHECK_ERR(
      shadowDenoise->Send(previousDepthBufferKey, textures.PreviousDepth));

  auto countKey = Graphics::ResourceKey{"DirectionalLightCount"};
  CHECK_ERR(Graphics::UniformWriter::Send(
      lightingShader, countKey,
      renderer.GetSceneLightBuffers().DirectionalLightCount));
  CHECK_ERR(Graphics::UniformWriter::Send(
      forward, countKey,
      renderer.GetSceneLightBuffers().DirectionalLightCount));

  CHECK_ERR(renderer.BindLightBuffers(ctx, lightingShader));
  CHECK_ERR(renderer.BindLightBuffers(ctx, forward));

  textures.ShadowHitFlags =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.ShadowHitFlags));

  // clang-format off
  static Graphics::ResourceKey LightIndexKey = {"PushConstants", "LightIndex"};
  static Graphics::ResourceKey LayerOffsetKey = {"PushConstants", "LayerOffset"};
  static Graphics::ResourceKey OutputTextureKey = {"OutputTexture"};
  static Graphics::ResourceKey PreviousOutputTextureKey = {"PreviousOutputTexture"};
  static Graphics::ResourceKey MotionTextureKey = {"MotionTexture"};
  static Graphics::ResourceKey RayHitFlagsTextureKey = {"RayHitTexture"};

  static auto blueNoiseTextureKey = Graphics::ResourceKey{"BlueNoiseTexture"};
  auto &blueNoiseTex = renderer.GetBlueNoiseTexture();


  auto shadowDirectional = CHECK_RES(manager.GetShader(K::Shadows_Directional));
  CHECK_ERR(shadowDirectional->Send(depthBufferKey, textures.Depth));
  CHECK_ERR(shadowDirectional->Send(normalTextureKey, textures.Normal));

  static auto rndStateKey =
      Graphics::ResourceKey{"PushConstants", "FrameRandomState"};
  auto frameRandomState = Math::Random(0, 255); // NOLINT
  CHECK_ERR(Graphics::UniformWriter::Send(shadowDirectional, rndStateKey,
                                          frameRandomState));

  // CHECK_ERR(shadowPoint->Send(blueNoiseTextureKey, blueNoiseTex));
  // CHECK_ERR(shadowSpot->Send(blueNoiseTextureKey, blueNoiseTex));
  // CHECK_ERR(shadowSphere->Send(blueNoiseTextureKey, blueNoiseTex));
  // CHECK_ERR(shadowRectangle->Send(blueNoiseTextureKey, blueNoiseTex));
  CHECK_ERR(forward->Send(blueNoiseTextureKey, blueNoiseTex));

  CHECK_ERR(
      Graphics::UniformWriter::Send(lightingShader, rndStateKey, frameRandomState));
  CHECK_ERR(
      Graphics::UniformWriter::Send(forward, rndStateKey, frameRandomState));

  static auto tlasKey = Graphics::ResourceKey{"SceneBVH"};
  CHECK_ERR(shadowDirectional->Send(tlasKey, renderer.GetSceneTLAS()));
  CHECK_ERR(shadowDirectional->Send(blueNoiseTextureKey, blueNoiseTex));
  CHECK_ERR(forward->Send(tlasKey, renderer.GetSceneTLAS()));

  CHECK_ERR(renderer.BindLightBuffers(ctx, shadowDirectional));

  CHECK_ERR(Graphics::UniformWriter::Send(shadowDirectional, LayerOffsetKey, 0U));
  CHECK_ERR(Graphics::UniformWriter::Send(shadowDirectional, LightIndexKey, 0U));
  CHECK_ERR(shadowDirectional->Send(cameraBufferKey, cameraBuffer));
  CHECK_ERR(shadowDirectional->Send(OutputTextureKey, textures.ShadowHitFlags));

  auto shadowsReset = CHECK_RES(manager.GetShader(Renderer::ShaderKey::Shadows_ClearHitFlags));
  CHECK_ERR(shadowsReset->Send(OutputTextureKey, textures.ShadowHitFlags));

  auto width = textures.ShadowHitFlags->GetWidth();
  auto height = textures.ShadowHitFlags->GetHeight();

  Graphics::RenderState::SetShader(shadowsReset);

  CHECK_ERR(Graphics::DispatchWithin(ctx, Math::Uvec3(width / 4, height / 4, 1)));

  Graphics::RenderState::SetShader(shadowDirectional);

  CHECK_ERR(Graphics::DispatchWithin(ctx, Math::Uvec3(width, height, 1)));

  textures.ShadowVisibility =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.ShadowVisibility));

  if (!textures.PreviousShadowVisibility.isValid()) {
    textures.PreviousShadowVisibility =
        CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
            context, rendertargets.ShadowVisibility));
  }

  CHECK_ERR(Graphics::UniformWriter::Send(shadowDenoise, LayerOffsetKey, 0U));
  CHECK_ERR(shadowDenoise->Send(cameraBufferKey, cameraBuffer));
  CHECK_ERR(shadowDenoise->Send(OutputTextureKey, textures.ShadowVisibility));
  CHECK_ERR(shadowDenoise->Send(MotionTextureKey, textures.Motion));
  CHECK_ERR(shadowDenoise->Send(PreviousOutputTextureKey,
                                textures.PreviousShadowVisibility));
  CHECK_ERR(
      shadowDenoise->Send(RayHitFlagsTextureKey, textures.ShadowHitFlags));

  Graphics::RenderState::SetShader(shadowDenoise);

  CHECK_ERR(Graphics::DispatchWithin(ctx, Math::Uvec3(width, height, 1)));

  // clang-format on

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Ambient Occlusion");

  textures.AmbientOcclusionSamples =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.AmbientOcclusionSamples));
  if (!textures.PreviousAmbientOcclusion.isValid()) {
    textures.PreviousAmbientOcclusion =
        CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
            context, rendertargets.AmbientOcclusion));
  }

  auto ambientOcclusionShader =
      CHECK_RES(manager.GetShader(Renderer::ShaderKey::RTAO));
  CHECK_ERR(ambientOcclusionShader->Send(depthBufferKey, textures.Depth));
  CHECK_ERR(ambientOcclusionShader->Send(normalTextureKey, textures.Normal));
  CHECK_ERR(ambientOcclusionShader->Send(blueNoiseTextureKey, blueNoiseTex));
  CHECK_ERR(ambientOcclusionShader->Send(cameraBufferKey, cameraBuffer));
  CHECK_ERR(ambientOcclusionShader->Send(tlasKey, renderer.GetSceneTLAS()));
  CHECK_ERR(ambientOcclusionShader->Send(OutputTextureKey,
                                         textures.AmbientOcclusionSamples));
  CHECK_ERR(Graphics::UniformWriter::Send(ambientOcclusionShader, rndStateKey,
                                          frameRandomState));

  Graphics::RenderState::SetShader(ambientOcclusionShader);

  CHECK_ERR(Graphics::DispatchWithin(ctx, Math::Uvec3(width, height, 1)));

  textures.AmbientOcclusion =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.AmbientOcclusion));

  auto aoDenoiseShader =
      CHECK_RES(manager.GetShader(Renderer::ShaderKey::AO_Denoise));
  CHECK_ERR(aoDenoiseShader->Send(depthBufferKey, textures.Depth));
  CHECK_ERR(
      aoDenoiseShader->Send(previousDepthBufferKey, textures.PreviousDepth));
  CHECK_ERR(aoDenoiseShader->Send(MotionTextureKey, textures.Motion));
  CHECK_ERR(aoDenoiseShader->Send(RayHitFlagsTextureKey,
                                  textures.AmbientOcclusionSamples));
  CHECK_ERR(aoDenoiseShader->Send(cameraBufferKey, cameraBuffer));
  CHECK_ERR(aoDenoiseShader->Send(PreviousOutputTextureKey,
                                  textures.PreviousAmbientOcclusion));
  CHECK_ERR(aoDenoiseShader->Send(OutputTextureKey, textures.AmbientOcclusion));

  Graphics::RenderState::SetShader(aoDenoiseShader);

  CHECK_ERR(Graphics::DispatchWithin(ctx, Math::Uvec3(width, height, 1)));

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Lighting");

  CHECK_ERR(Renderer::GlobalRenderTargetManager.ReleaseRendertargets(
      {textures.PreviousShadowVisibility, textures.ShadowHitFlags,
       textures.AmbientOcclusionSamples, textures.PreviousAmbientOcclusion}));
  textures.PreviousShadowVisibility = textures.ShadowVisibility;
  textures.PreviousAmbientOcclusion = textures.AmbientOcclusion;

  Graphics::RenderState::SetShader(lightingShader);
  static Graphics::ResourceKey ShadowVisibilityTextureKey = {
      "ShadowVisibilityTexture"};
  CHECK_ERR(lightingShader->Send(ShadowVisibilityTextureKey,
                                 textures.ShadowVisibility));

  textures.DirectLighting =
      CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(
          context, rendertargets.DirectLighting));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      context, {{
                   .texture = textures.DirectLighting,
                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
               }}));

  // TODO: Remove Normals, Motion and Material here once we need them for post-processing effects
  CHECK_ERR(Renderer::GlobalRenderTargetManager.ReleaseRendertargets({
      textures.Albedo,
      textures.Normal,
      textures.Material,
      textures.Emissive,
      textures.Motion,
  }));

  // Now left: IncomingLight and Depth

  Graphics::RenderState::SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

  CHECK_ERR(Renderer::DrawFullScreen(context));

  CHECK_ERR(Graphics::RenderState::SetRenderTargets(
      ctx, {{
                .texture = textures.Depth,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            },
            {
                .blendMode = Graphics::DefaultBlendMode,
                .texture = textures.DirectLighting,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            }}));

  static auto forwardConfig = DrawConfig{
      .bindMaterialTextures = UINT8_MAX,
      .bindMaterialBuffer = true,
  };

  Graphics::PopDebugMarker();
  Graphics::PushDebugMarker("Transparent Materials");

  Graphics::RenderState::SetWindingOrder(VK_FRONT_FACE_COUNTER_CLOCKWISE);

  // Skybox should be drawn before transparent objects, this is currently wrong.
  Graphics::RenderState::SetDepthMode(true, false, VK_COMPARE_OP_GREATER);
  Graphics::RenderState::SetShader(forward);
  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);
  for (const auto &item : TransparentDrawItems) {
    CHECK_ERR(RenderDrawItem(item, forward, ctx, forwardConfig));
  }

  Graphics::PopDebugMarker();

  Graphics::RenderState::SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

  // Otherwise meshes won't be destroyed due to living in these vectors
  OpaqueDrawItems.clear();
  MaskedDrawItems.clear();
  TransparentDrawItems.clear();

  return {};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
Scene::Scene(std::string name) : name(std::move(name)) {
  AddGuiMethod<Transform>(&Transform::DrawGUI, true);
  AddGuiMethod<Camera>(&Camera::DrawGUI, true);
  AddGuiMethod<Renderer::Material>(&Renderer::Material::DrawGUI, true);
  AddGuiMethod<LocalBounds>(&LocalBounds::DrawGUI);
  AddGuiMethod<WorldBounds>(&WorldBounds::DrawGUI);
  AddGuiMethod<Node>(&Node::DrawGUI);
  AddGuiMethod<Light>(&Light::DrawGUI, true);

  AddGuiMethod<Model>(&Model::DrawGUI);
  AddGuiMethod<Shape>(&Shape::DrawGUI);
  AddGuiMethod<LevelOfDetail>(&LevelOfDetail::DrawGUI);
  AddGuiMethod<Geometry>(&Geometry::DrawGUI);
  AddGuiMethod<Renderer::LightProbe>(&Renderer::LightProbe::DrawGUI, true);

  auto transformSystem =
      world.system<Engine::Transform, Engine::Transform *>()
          .term_at(1)
          .up()      // Start at parents
          .cascade() // Breadth-first
          .each([](Transform &transform, Transform *parentTransform) -> auto {
            transform.UpdateLocalMatrix();
            transform.UpdateWorldMatrix(parentTransform);
          });

  auto postTransformSystem = world.system<Engine::Transform>().each(
      [](Transform &transform) -> auto { transform.MarkChildrenUpdated(); });

  postTransformSystem.depends_on(transformSystem);

  auto preBoundsCascadeSystem = world.system<Engine::WorldBounds>().each(
      [](Engine::WorldBounds &bbox) -> auto { bbox.Bounds.Reset(); });

  preBoundsCascadeSystem.depends_on(transformSystem);

  auto boundingBoxSystem =
      world
          .system<Engine::Transform, Engine::LocalBounds, Engine::WorldBounds>()
          .each([](flecs::entity entity, Engine::Transform &transform,
                   Engine::LocalBounds &bbox,
                   Engine::WorldBounds &wbbox) -> auto {
            wbbox.Bounds.Construct(transform, bbox.Bounds);
          });
  boundingBoxSystem.depends_on(preBoundsCascadeSystem);

  auto postBoundingboxSystem =
      world.system<Engine::WorldBounds, Engine::WorldBounds *>()
          .term_at(1)
          .up()      // Start at parents
          .cascade() // Breadth-first
          .desc()    // Reverse order, so children are processed before parents
          .each([](Engine::WorldBounds &bbox,
                   Engine::WorldBounds *parentBbox) -> auto {
            if (parentBbox != nullptr) {
              parentBbox->Bounds.UnionInPlace(bbox.Bounds);
            }
          });

  postBoundingboxSystem.depends_on(boundingBoxSystem);

  Camera::RegisterCameraSystems(*this);

  auto &instance = Renderer::RendererInstance;
  auto &buffers = instance.GetSceneLightBuffers();

  preRender.emplace_back(world.system<Renderer::Material>().kind(0).each(
      [this](Renderer::Material &material) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult =
            material.Update(*Graphics::GetCurrentGraphicsContext());
      }));

  preRender.emplace_back(world.system<DirectionalLight>().kind(0).each(
      [this, &buffers](flecs::entity entity,
                       const DirectionalLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.DirectionalLightData, entity);
      }));

  preRender.emplace_back(world.system<PointLight>().kind(0).each(
      [this, &buffers](flecs::entity entity, const PointLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.PointLightData, entity);
      }));

  preRender.emplace_back(world.system<SpotLight>().kind(0).each(
      [this, &buffers](flecs::entity entity, const SpotLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.SpotLightData, entity);
      }));

  preRender.emplace_back(world.system<RectangleLight>().kind(0).each(
      [this, &buffers](flecs::entity entity,
                       const RectangleLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.RectangleLightData, entity);
      }));

  preRender.emplace_back(world.system<SphereLight>().kind(0).each(
      [this, &buffers](flecs::entity entity, const SphereLight &light) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        lastUpdateResult = light.Write(buffers.SphereLightData, entity);
      }));

  preRender.emplace_back(world.system<Camera>().kind(0).each(
      [this](flecs::entity entity, const Camera &camera) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        auto &transform = entity.get_mut<Transform>();
        auto &cameraMatrices = entity.get_mut<CameraMatrices>();

        lastUpdateResult = camera.WriteToBuffer(cameraMatrices, transform);
      }));

  preRender.emplace_back(world.system<Geometry>().kind(0).each(
      [this](flecs::entity entity, Geometry &geometry) -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        auto &transform = entity.get_mut<Transform>();
        const auto &worldMatrix = transform.GetWorldMatrix();

        auto &renderer = Renderer::RendererInstance;

        if (geometry.mesh->GetBLAS() != nullptr) {
          if (!geometry.hasTlasIndex) {
            geometry.hasTlasIndex = true;

            auto result = renderer.GetSceneTLAS()->AddInstance(
                geometry.mesh->GetBLAS(), worldMatrix);

            if (Error::IsError(result)) {
              lastUpdateResult = result.error();
              return;
            }

            geometry.tlasIndex = result.value();

            renderer.SetSceneNeedsTLASRebuild(true);
          } else {
            renderer.GetSceneTLAS()->UpdateInstance(geometry.tlasIndex,
                                                    worldMatrix);
          }
        }
      }));

  finalizePreRenderUploads =
      world.system().kind(0).each([this, &buffers]() -> auto {
        if (lastUpdateResult.IsError()) {
          return;
        }

        auto &ctx = *Graphics::GetCurrentGraphicsContext();
        auto &renderer = Renderer::RendererInstance;

        Error error = Graphics::BVHManagerInstance.Update(ctx);

        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }

        if (!renderer.GetSceneTLAS()->GetInstances().empty()) {
          error = renderer.SceneNeedsTLASRebuild()
                      ? renderer.GetSceneTLAS()->Rebuild(ctx)
                      : renderer.GetSceneTLAS()->Refit(ctx);
          renderer.SetSceneNeedsTLASRebuild(false);

          if (Error::IsError(error)) {
            lastUpdateResult = error;
            return;
          }
        }

        error = buffers.DirectionalLightsBuffer->GetBuffer()->SetData(
            ctx, buffers.DirectionalLightData);
        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }

        error = buffers.PointLightsBuffer->GetBuffer()->SetData(
            ctx, buffers.PointLightData);
        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }

        error = buffers.SpotLightsBuffer->GetBuffer()->SetData(
            ctx, buffers.SpotLightData);
        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }

        error = buffers.RectangleLightsBuffer->GetBuffer()->SetData(
            ctx, buffers.RectangleLightData);
        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }

        error = buffers.SphereLightsBuffer->GetBuffer()->SetData(
            ctx, buffers.SphereLightData);
        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }

        error = renderer.GetMaterialUploadManager().Flush(ctx);
        if (Error::IsError(error)) {
          lastUpdateResult = error;
          return;
        }
      });
}

auto Scene::Update(double deltaTime) const -> Error {
  world.progress(static_cast<float>(deltaTime));
  Renderer::GlobalRenderTargetManager.Update();

  return lastUpdateResult;
}

auto Scene::SetEnvironment(flecs::entity environment) -> void {
  currentEnvironment = environment;
}

auto Scene::GetEnvironment() const -> flecs::entity {
  return currentEnvironment;
}

auto LuaScene::Update(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));

  auto deltaTime = luaL_checknumber(state, 2);
  auto updateResult = scene->Update(deltaTime);
  if (Error::IsError(updateResult)) {
    return luaL_error(state, "%s", updateResult.ToString().c_str());
  }

  return 0;
}

auto LuaScene::DrawUiElement(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));

  auto drawResult = DrawSceneHierarchy(scene);
  if (Error::IsError(drawResult)) {
    return luaL_error(state, "%s", drawResult.ToString().c_str());
  }

  return 0;
}

auto LuaScene::SetEnvironment(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));
  auto *environment = LUA_CK_NULL(::LuaWrap::EntityFromLua(state, 2));

  scene->SetEnvironment(*environment);

  return 0;
}

const ::LuaWrap::LuaClass SceneLuaClass{
    .Name = "Scene",
    .Type = Scene::GetType(),
    .Methods =
        {
            {"drawUIElement", LuaScene::DrawUiElement},
            {"update", LuaScene::Update},
            {"newModel", LuaModel::Create},
            {"newShape", LuaShape::Create},
            {"newLOD", LuaLevelOfDetail::Create},
            {"newGeometry", LuaGeometry::Create},
            {"newDirectionalLight", LuaDirectionalLight::Create},
            {"newPointLight", LuaPointLight::Create},
            {"newSpotLight", LuaSpotLight::Create},
            {"newRectangleLight", LuaRectangleLight::Create},
            {"newSphereLight", LuaSphereLight::Create},
            {"newCamera", LuaCamera::Create},
            {"setEnvironment", LuaScene::SetEnvironment},
            {"newEnvironment", LuaEnvironment::Create},
            {"newLightProbe", Renderer::LuaLightProbe::Create},
        },
    .Children = {},
};

} // namespace Engine