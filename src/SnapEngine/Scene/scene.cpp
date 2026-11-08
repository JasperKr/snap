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
#include "Modules/Helpers/utils.hpp"
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
#include "Scene/volume.hpp"
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

using namespace Graphics;

auto LuaScene::LoadBinding(lua_State *state) -> int {
  Bindings::LuaBoundStruct<Scene> bindings("Scene");
  bindings.RegisterMember<&Scene::name>("Name");
  bindings.DocumentCustomMethod(Bindings::MethodInfo{.name = "DrawUiElement", .description = "Draws a UI element for this scene."});
  bindings.Register(state);

  return 0;
}

inline auto CompareDrawItems(const DrawItem &first, const DrawItem &second) -> bool {
  if (first.primaryKey != second.primaryKey) {
    return first.primaryKey < second.primaryKey;
  }
  if (first.secondaryKey != second.secondaryKey) {
    return first.secondaryKey < second.secondaryKey;
  }
  return first.tertiaryKey < second.tertiaryKey;
}

inline auto BindMaterial(const GraphicsContext &context, const Ref<Shader> &shader, Renderer::Material const *material, uint8_t flags) -> Error {
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
    CHECK_ERR(shader->Send(context, "AlbedoTexture", albedoTexture));
  }

  if ((flags & 2U) != 0U) {
    CHECK_ERR(shader->Send(context, "MetallicRoughnessTexture", metallicRoughnessTexture));
  }

  // if ((flags & 4U) != 0U) {
  //   CHECK_ERR(
  //       shader->Send(context, "AmbientOcclusionTexture", ambientOcclusionTexture));
  // }

  if ((flags & 8U) != 0U) {
    CHECK_ERR(shader->Send(context, "NormalTexture", normalTexture));
  }

  if ((flags & 16U) != 0U) {
    CHECK_ERR(shader->Send(context, "EmissiveTexture", emissiveTexture));
  }

  if ((flags & 32U) != 0U) {
    CHECK_ERR(shader->Send(context, "ReflectanceTexture", reflectanceTexture));
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

inline auto RenderDrawItem(const DrawItem &item, const Ref<Shader> &shader, const GraphicsContext &ctx, const DrawConfig &config) -> Error {
  if (config.bindMaterialTextures != 0) {
    CHECK_ERR(BindMaterial(ctx, shader, item.material, config.bindMaterialTextures));
  }

  if (config.bindMaterialBuffer) {
    CHECK_ERR(UniformWriter::Send(shader, {"MaterialBufferIndex"}, item.material->materialSSBOIndex));
  }

  CHECK_ERR(UniformWriter::Send(shader, {"ModelTransformIndex"}, item.transformIndex));

  if (item.geometry.mesh.get() == nullptr) {
    return Error::Create("Invalid geometry mesh");
  }

  RenderState::SetCullMode(item.material->cullMode);

  CHECK_ERR(Draw(ctx, *item.geometry.mesh));

  return {};
}

inline auto AddDrawItem(std::vector<DrawItem> &OpaqueDrawItems, std::vector<DrawItem> &MaskedDrawItems, std::vector<DrawItem> &TransparentDrawItems,
                        flecs::entity entity, const Geometry &geometry, const Frustum &frustum) -> void {
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

auto Scene::PrepareDrawLists(const GraphicsContext &context, Frustum &frustum) -> Error {
  auto &renderer = Renderer::RendererInstance;

  renderer.NewFrame();

  for (const auto &system : preRender) {
    system.run();
  }
  finalizePreRenderUploads.run();

  OpaqueDrawItems.clear();
  MaskedDrawItems.clear();
  TransparentDrawItems.clear();

  world.each<Geometry>([&](flecs::entity entity, const Geometry &geometry) -> void {
    bool hasChildren = false;
    entity.children([&](flecs::entity child) -> void {
      hasChildren = true;

      AddDrawItem(OpaqueDrawItems, MaskedDrawItems, TransparentDrawItems, child, geometry, frustum);
    });

    if (!hasChildren) {
      AddDrawItem(OpaqueDrawItems, MaskedDrawItems, TransparentDrawItems, entity, geometry, frustum);
    }
  });

  std::ranges::sort(OpaqueDrawItems, CompareDrawItems);
  std::ranges::sort(MaskedDrawItems, CompareDrawItems);
  std::ranges::sort(TransparentDrawItems, CompareDrawItems);

  // Prepare model transform buffer

  struct ModelTransformData {
    Math::Matrix4x4 modelMatrix{};  // NOLINT
    Math::Matrix4x4 normalMatrix{}; // NOLINT

    constexpr explicit ModelTransformData(Transform transform) : modelMatrix(transform.GetWorldMatrix()) {
      normalMatrix = Math::Matrix4x4(transform.GetNormalMatrix());
    }
  };

  static std::vector<ModelTransformData> modelTransforms;
  snap_defer(modelTransforms.clear());
  size_t transformCount = 0UL;
  modelTransforms.reserve(OpaqueDrawItems.size() + MaskedDrawItems.size() + TransparentDrawItems.size());

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

  if (!modelTransforms.empty()) {
    auto span = std::span<const ModelTransformData>(modelTransforms.data(), modelTransforms.size());
    CHECK_ERR(renderer.GetModelTransformsBuffer()->GetBuffer()->SetData(context, span));
  }

  return {};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Scene::DrawOpaqueModels(const GraphicsContext &context, Camera &camera) -> Error {
  ZoneScoped;

  auto &renderer = Renderer::RendererInstance;

  // Shader configuration

  auto &textures = camera.GetOwnedTextures();
  auto &manager = renderer.GetShaderManager();
  auto cameraBuffer = camera.GetBuffer();
  auto transforms = renderer.GetModelTransformsBuffer();
  auto materialBuffer = renderer.GetMaterialsBuffer();

  auto depthOpaque = CHECK_RES(manager.GetShader(Renderer::ShaderKey::DepthPrepass));
  auto depthMasked = CHECK_RES(manager.GetShader(Renderer::ShaderKey::DepthMaskPass));
  auto deferred = CHECK_RES(manager.GetShader(Renderer::ShaderKey::Deferred));

  CHECK_ERR(depthOpaque->Send(context, "CameraData", {cameraBuffer, Graphics::StorageBuffer::read}));
  CHECK_ERR(depthMasked->Send(context, "CameraData", {cameraBuffer, Graphics::StorageBuffer::read}));
  CHECK_ERR(deferred->Send(context, "CameraData", {cameraBuffer, Graphics::StorageBuffer::read}));

  CHECK_ERR(depthOpaque->Send(context, "ModelTransforms", {transforms, StorageBuffer::read}));
  CHECK_ERR(depthMasked->Send(context, "ModelTransforms", {transforms, StorageBuffer::read}));
  CHECK_ERR(deferred->Send(context, "ModelTransforms", {transforms, StorageBuffer::read}));

  CHECK_ERR(depthMasked->Send(context, "MaterialBuffer", {materialBuffer, StorageBuffer::read}));
  CHECK_ERR(deferred->Send(context, "MaterialBuffer", {materialBuffer, StorageBuffer::read}));

  // Draw sorting

  const auto &rendertargets = camera.GetRendertargets();

  // Depth Opaque prepass

  textures.Depth = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Depth));

  RenderState::SetDepthMode(true, true, VK_COMPARE_OP_GREATER);
  RenderState::SetWindingOrder(VK_FRONT_FACE_COUNTER_CLOCKWISE);

  CHECK_ERR(RenderState::SetRenderTargets(context, {{
                                                       .clearValue = VkClearValue{0.0F, 0},
                                                       .texture = textures.Depth,
                                                       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                   }}));

  auto opaqueConfig = DrawConfig{
      .bindMaterialTextures = 0,
      .bindMaterialBuffer = false,
  };

  Graphics::RenderState::PushDebugMarker("Depth Opaque Prepass");

  RenderState::SetShader(depthOpaque);
  for (const auto &item : OpaqueDrawItems) {
    CHECK_ERR(RenderDrawItem(item, depthOpaque, context, opaqueConfig));
  }

  Graphics::RenderState::PopDebugMarker();
  Graphics::RenderState::PushDebugMarker("Depth Masked Prepass");

  auto maskedConfig = DrawConfig{
      .bindMaterialTextures = 1,
      .bindMaterialBuffer = true,
  };
  RenderState::SetShader(depthMasked);
  for (const auto &item : MaskedDrawItems) {
    CHECK_ERR(RenderDrawItem(item, depthMasked, context, maskedConfig));
  }

  Graphics::RenderState::PopDebugMarker();

  textures.Albedo = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Albedo));
  textures.Normal = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Normal));
  textures.Material = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Material));
  textures.Emissive = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Emissive));
  textures.Motion = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Motion));

  RenderState::SetDepthMode(true, false, VK_COMPARE_OP_EQUAL);
  RenderState::SetShader(deferred);

  CHECK_ERR(RenderState::SetRenderTargets(context, {{
                                                        .texture = textures.Depth,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                    },
                                                    {
                                                        .blendMode = BlendmodeNone,
                                                        .texture = textures.Albedo,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                    },
                                                    {
                                                        .blendMode = BlendmodeNone,
                                                        .texture = textures.Normal,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                    },
                                                    {
                                                        .blendMode = BlendmodeNone,
                                                        .texture = textures.Material,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                    },
                                                    {
                                                        .blendMode = BlendmodeNone,
                                                        .texture = textures.Emissive,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                    },
                                                    {
                                                        .blendMode = BlendmodeNone,
                                                        .texture = textures.Motion,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                    }}));

  auto deferredConfig = DrawConfig{
      .bindMaterialTextures = UINT8_MAX,
      .bindMaterialBuffer = true,
  };

  Graphics::RenderState::PushDebugMarker("Opaque Materials");

  for (const auto &item : OpaqueDrawItems) {
    CHECK_ERR(RenderDrawItem(item, deferred, context, deferredConfig));
  }

  Graphics::RenderState::PopDebugMarker();
  Graphics::RenderState::PushDebugMarker("Masked Materials");

  for (const auto &item : MaskedDrawItems) {
    CHECK_ERR(RenderDrawItem(item, deferred, context, deferredConfig));
  }

  if (OpaqueDrawItems.empty() && MaskedDrawItems.empty()) {
    std::array<VkClearColorValue, 5> colors = {
        VkClearColorValue{0.0F, 0.0F, 0.0F, 1.0F}, // Albedo
        VkClearColorValue{0.0F, 0.0F, 0.0F, 1.0F}, // Normal
        VkClearColorValue{0.0F, 0.0F, 0.0F, 1.0F}, // Material
        VkClearColorValue{0.0F, 0.0F, 0.0F, 1.0F}, // Emissive
        VkClearColorValue{0.0F, 0.0F, 0.0F, 1.0F}, // Motion
    };

    CHECK_ERR(RenderState::Clear(context, colors.data(), colors.size(), VkClearDepthStencilValue{.depth = 0.0F}, true, false));
  }

  Graphics::RenderState::PopDebugMarker();

  RenderState::SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

  OpaqueDrawItems.clear();
  MaskedDrawItems.clear();

  return {};
}

auto Scene::DrawTransparentModels(const GraphicsContext &context, Camera &camera) -> Error {
  auto &renderer = Renderer::RendererInstance;

  auto &textures = camera.GetOwnedTextures();
  auto &manager = renderer.GetShaderManager();
  auto cameraBuffer = camera.GetBuffer();
  auto transforms = renderer.GetModelTransformsBuffer();
  auto materialBuffer = renderer.GetMaterialsBuffer();

  auto forward = CHECK_RES(manager.GetShader(Renderer::ShaderKey::TransparencyForward));
  CHECK_ERR(forward->Send(context, "MaterialBuffer", {materialBuffer, StorageBuffer::read}));
  CHECK_ERR(UniformWriter::Send(forward, "DirectionalLightCount", renderer.GetSceneLightBuffers().DirectionalLightCount));
  CHECK_ERR(renderer.BindLightBuffers(context, forward));
  CHECK_ERR(forward->Send(context, "CameraData", {cameraBuffer, Graphics::StorageBuffer::read}));
  CHECK_ERR(forward->Send(context, "ModelTransforms", {transforms, StorageBuffer::read}));
  CHECK_ERR(forward->Send(context, "BlueNoiseTexture", renderer.GetBlueNoiseTexture()));
  CHECK_ERR(forward->Send(context, "SceneBVH", renderer.GetSceneTLAS()));

  Graphics::RenderState::PushDebugMarker("Transparent Materials");

  RenderState::SetWindingOrder(VK_FRONT_FACE_COUNTER_CLOCKWISE);

  // Skybox should be drawn before transparent objects, this is currently wrong.
  RenderState::SetDepthMode(true, false, VK_COMPARE_OP_GREATER);
  RenderState::SetShader(forward);
  RenderState::SetCullMode(VK_CULL_MODE_NONE);

  CHECK_ERR(RenderState::SetRenderTargets(context, {{
                                                        .texture = textures.Depth,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                    },
                                                    {
                                                        .blendMode = DefaultBlendMode,
                                                        .texture = textures.IncomingLight,
                                                        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                                    }}));

  auto forwardConfig = DrawConfig{
      .bindMaterialTextures = UINT8_MAX,
      .bindMaterialBuffer = true,
  };

  for (const auto &item : TransparentDrawItems) {
    CHECK_ERR(RenderDrawItem(item, forward, context, forwardConfig));
  }

  RenderState::SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

  Graphics::RenderState::PopDebugMarker();

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
  AddGuiMethod<Engine::Volume>(&Engine::Volume::DrawGUI, true);

  auto transformSystem = world.system<Engine::Transform, Engine::Transform *>()
                             .term_at(1)
                             .up()      // Start at parents
                             .cascade() // Breadth-first
                             .each([](Transform &transform, Transform *parentTransform) -> auto {
                               transform.UpdateLocalMatrix();
                               transform.UpdateWorldMatrix(parentTransform);
                             });

  auto postTransformSystem = world.system<Engine::Transform>().each([](Transform &transform) -> auto { transform.MarkChildrenUpdated(); });

  postTransformSystem.depends_on(transformSystem);

  auto preBoundsCascadeSystem = world.system<Engine::WorldBounds>().each([](Engine::WorldBounds &bbox) -> auto { bbox.Bounds.Reset(); });

  preBoundsCascadeSystem.depends_on(transformSystem);

  auto boundingBoxSystem = world.system<Engine::Transform, Engine::LocalBounds, Engine::WorldBounds>().each(
      [](flecs::entity entity, Engine::Transform &transform, Engine::LocalBounds &bbox, Engine::WorldBounds &wbbox) -> auto {
        wbbox.Bounds.Construct(transform, bbox.Bounds);
      });
  boundingBoxSystem.depends_on(preBoundsCascadeSystem);

  auto postBoundingboxSystem = world.system<Engine::WorldBounds, Engine::WorldBounds *>()
                                   .term_at(1)
                                   .up()      // Start at parents
                                   .cascade() // Breadth-first
                                   .desc()    // Reverse order, so children are processed before parents
                                   .each([](Engine::WorldBounds &bbox, Engine::WorldBounds *parentBbox) -> auto {
                                     if (parentBbox != nullptr) {
                                       parentBbox->Bounds.UnionInPlace(bbox.Bounds);
                                     }
                                   });

  postBoundingboxSystem.depends_on(boundingBoxSystem);

  Camera::RegisterCameraSystems(*this);

  auto &instance = Renderer::RendererInstance;
  auto &buffers = instance.GetSceneLightBuffers();

  preRender.emplace_back(world.system<Renderer::Material>().kind(0).each([this](Renderer::Material &material) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    lastUpdateResult = material.Update(*GetCurrentGraphicsContext());
  }));

  preRender.emplace_back(world.system<DirectionalLight>().kind(0).each([this, &buffers](flecs::entity entity, const DirectionalLight &light) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    lastUpdateResult = light.Write(buffers.DirectionalLightData, entity);
  }));

  preRender.emplace_back(world.system<PointLight>().kind(0).each([this, &buffers](flecs::entity entity, const PointLight &light) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    lastUpdateResult = light.Write(buffers.PointLightData, entity);
  }));

  preRender.emplace_back(world.system<SpotLight>().kind(0).each([this, &buffers](flecs::entity entity, const SpotLight &light) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    lastUpdateResult = light.Write(buffers.SpotLightData, entity);
  }));

  preRender.emplace_back(world.system<RectangleLight>().kind(0).each([this, &buffers](flecs::entity entity, const RectangleLight &light) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    lastUpdateResult = light.Write(buffers.RectangleLightData, entity);
  }));

  preRender.emplace_back(world.system<SphereLight>().kind(0).each([this, &buffers](flecs::entity entity, const SphereLight &light) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    lastUpdateResult = light.Write(buffers.SphereLightData, entity);
  }));

  preRender.emplace_back(world.system<Camera>().kind(0).each([this](flecs::entity entity, const Camera &camera) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    auto &transform = entity.get_mut<Transform>();
    auto &cameraMatrices = entity.get_mut<CameraMatrices>();

    lastUpdateResult = camera.WriteToBuffer(cameraMatrices, transform);
  }));

  preRender.emplace_back(world.system<Geometry>().kind(0).each([this](flecs::entity entity, Geometry &geometry) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    auto &transform = entity.get_mut<Transform>();
    const auto &worldMatrix = transform.GetWorldMatrix();

    auto &renderer = Renderer::RendererInstance;

    if (geometry.mesh->GetBLAS() != nullptr) {
      if (!geometry.hasTlasIndex) {
        geometry.hasTlasIndex = true;

        auto result = renderer.GetSceneTLAS()->AddInstance(geometry.mesh->GetBLAS(), worldMatrix, 0);

        if (Error::IsError(result)) {
          lastUpdateResult = result.error();
          return;
        }

        geometry.tlasIndex = result.value();

        renderer.SetSceneNeedsTLASRebuild(true);
      } else {
        renderer.GetSceneTLAS()->UpdateInstance(geometry.tlasIndex, worldMatrix);
      }
    }
  }));

  preRender.emplace_back(world.system<Volume>().kind(0).each([this](flecs::entity entity, Volume &volume) -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    auto &transform = entity.get_mut<Transform>();
    const auto &worldMatrix = transform.GetWorldMatrix();
    auto &ctx = *GetCurrentGraphicsContext();

    auto &renderer = Renderer::RendererInstance;

    static BoundingBox localBox = {
        .Min = {-0.5, -0.5, -0.5},
        .Max = {0.5, 0.5, 0.5},
    };

    BoundingBox worldBox{};

    worldBox.Construct(transform, localBox);

    lastUpdateResult = renderer.GetVolumeManager().UpdateInstance(ctx, volume.tlasIndex, volume, worldBox.Min, worldBox.Max);

    if (lastUpdateResult.IsError()) {
      return;
    }
  }));

  finalizePreRenderUploads = world.system().kind(0).each([this, &buffers]() -> auto {
    if (lastUpdateResult.IsError()) {
      return;
    }

    auto &ctx = *GetCurrentGraphicsContext();
    auto &renderer = Renderer::RendererInstance;

    Error error = BVHManagerInstance.Update(ctx);

    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = renderer.GetVolumeManager().Update(ctx);

    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    if (!renderer.GetSceneTLAS()->GetInstances().empty()) {
      error = renderer.SceneNeedsTLASRebuild() ? renderer.GetSceneTLAS()->Rebuild(ctx) : renderer.GetSceneTLAS()->Refit(ctx);
      renderer.SetSceneNeedsTLASRebuild(false);

      if (Error::IsError(error)) {
        lastUpdateResult = error;
        return;
      }
    }

    error = buffers.DirectionalLightsBuffer->GetBuffer()->SetData(ctx, buffers.DirectionalLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.PointLightsBuffer->GetBuffer()->SetData(ctx, buffers.PointLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.SpotLightsBuffer->GetBuffer()->SetData(ctx, buffers.SpotLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.RectangleLightsBuffer->GetBuffer()->SetData(ctx, buffers.RectangleLightData);
    if (Error::IsError(error)) {
      lastUpdateResult = error;
      return;
    }

    error = buffers.SphereLightsBuffer->GetBuffer()->SetData(ctx, buffers.SphereLightData);
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

auto Scene::SetEnvironment(flecs::entity environment) -> void { currentEnvironment = environment; }

auto Scene::GetEnvironment() const -> flecs::entity { return currentEnvironment; }

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
            {"newVolume", Engine::LuaVolume::Create},
        },
    .Children = {},
};

} // namespace Engine