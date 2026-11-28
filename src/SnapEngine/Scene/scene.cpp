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

#include "Jolt/Jolt.h" // IWYU pragma: keep
#include "Jolt/Physics/PhysicsSystem.h"

#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/bindings.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Renderer/lightProbe.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/Geometry/Shapes/boxShape.hpp"
#include "Scene/Geometry/Shapes/capsuleShape.hpp"
#include "Scene/Geometry/Shapes/convexHullShape.hpp"
#include "Scene/Geometry/Shapes/cylinderShape.hpp"
#include "Scene/Geometry/Shapes/heightFieldShape.hpp"
#include "Scene/Geometry/Shapes/meshShape.hpp"
#include "Scene/Geometry/Shapes/planeShape.hpp"
#include "Scene/Geometry/Shapes/sphereShape.hpp"
#include "Scene/Geometry/Shapes/taperedCapsuleShape.hpp"
#include "Scene/Geometry/Shapes/taperedCylinderShape.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/model.hpp"
#include "Scene/Geometry/rigidbody.hpp"
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
#include "defer.hpp"
#include "material.hpp"
#include "renderer.hpp"
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

inline auto RenderDrawItem(const DrawItem &item, const Ref<Shader> &shader, const GraphicsContext &ctx, ResolvedKey key) -> Error {
  CHECK_ERR(UniformWriter::Send(shader, key, Math::Uvec2{item.transformIndex, item.material->materialSSBOIndex}));

  if (item.geometry.mesh.get() == nullptr) {
    return Error::Create("Invalid geometry mesh");
  }

  RenderState::SetCullMode(item.material->cullMode);

  CHECK_ERR(Draw(ctx, *item.geometry.mesh));

  return {};
}

auto Scene::PrepareDrawLists(const GraphicsContext &context, const Frustum &frustum, const Camera &camera) -> Error {
  auto &renderer = Renderer::RendererInstance;
  auto &manager = renderer.GetShaderManager();

  CHECK_ERR(renderer.NewFrame(context));
  CHECK_ERR(renderer.DrawBuffers.DrawIndices->Clear(context, 0));

  for (const auto &system : preRender) {
    system.run();
  }
  finalizePreRenderUploads.run();

  // Prepare model transform buffer

  struct ModelTransformData {
    ModelTransformData() = default;

    Math::Matrix4x4 modelMatrix{};  // NOLINT
    Math::Matrix4x4 normalMatrix{}; // NOLINT

    constexpr explicit ModelTransformData(const Transform &transform) : modelMatrix(transform.GetWorldMatrix()) {
      normalMatrix = Math::Matrix4x4(transform.GetNormalMatrix());
    }
  };

  static std::vector<ModelTransformData> modelTransforms;
  size_t count = renderer.TransformAllocator.backingAllocationSize / renderer.TransformAllocator.itemSize;
  modelTransforms.resize(count);

  world.each<Geometry>([&](flecs::entity entity, const Geometry &geometry) -> void {
    modelTransforms.at(geometry.transformIndex) = ModelTransformData(entity.get_mut<Transform>());
  });

  CHECK_ERR(renderer.AssureModelTransformBufferSize(count));

  if (!modelTransforms.empty()) {
    auto span = std::span<const ModelTransformData>(modelTransforms.data(), modelTransforms.size());
    CHECK_ERR(renderer.GetModelTransformsBuffer()->GetBuffer()->SetData(context, span));
  }

  auto cullRough = CHECK_RES(manager.GetShader(Renderer::ShaderKey::CullRough));

  auto indirect = CHECK_RES(manager.GetShader(Renderer::ShaderKey::Indirect));
  RenderState::SetShader(cullRough);

  for (uint32_t i = 0; i < renderer.DrawBuffers.DrawLists.size(); i++) {
    CHECK_ERR(cullRough->Send(context, KeyElement{"DrawItems", i}, {renderer.DrawBuffers.DrawLists.at(i), StorageBuffer::write}));
  }

  CHECK_ERR(cullRough->Send(context, "DrawItemCount", {renderer.DrawBuffers.DrawIndices, StorageBuffer::readwrite}));
  CHECK_ERR(indirect->Send(context, "DrawItemCount", {renderer.DrawBuffers.DrawIndices, StorageBuffer::read}));

  CHECK_ERR(indirect->Send(context, "DrawInfo", {renderer.DrawBuffers.DrawCommands, StorageBuffer::write}));

  CHECK_ERR(cullRough->Send(context, "Meshes", {renderer.MeshBuffer->GetBuffer(), StorageBuffer::read}));
  CHECK_ERR(cullRough->Send(context, "CameraData", {camera.GetBuffer(), StorageBuffer::read}));
  CHECK_ERR(cullRough->Send(context, "MaterialBuffer", {renderer.GetMaterialsBuffer()->GetBuffer(), StorageBuffer::read}));
  CHECK_ERR(cullRough->Send(context, "ModelTransforms", {renderer.GetModelTransformsBuffer()->GetBuffer(), StorageBuffer::read}));

  CHECK_ERR(DispatchWithin(context, {static_cast<uint32_t>(count), 1, 1}));

  RenderState::SetShader(indirect);
  CHECK_ERR(DispatchWithin(context, {renderer.DrawBuffers.DrawLists.size(), 1, 1}));

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

  textures.Depth = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Depth)).GetTexture();

  RenderState::SetDepthMode(true, true, VK_COMPARE_OP_GREATER);
  RenderState::SetWindingOrder(VK_FRONT_FACE_COUNTER_CLOCKWISE);

  CHECK_ERR(RenderState::SetRenderTargets(context, {{
                                                       .clearValue = VkClearValue{0.0F, 0},
                                                       .texture = textures.Depth,
                                                       .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                   }}));

  Graphics::RenderState::PushDebugMarker("Depth Opaque Prepass");
  RenderState::SetShader(depthOpaque);

  const auto stride = sizeof(VkDrawMeshTasksIndirectCommandEXT);

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);
  CHECK_ERR(depthOpaque->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(0), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 0 * stride));

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_BACK_BIT);
  CHECK_ERR(depthOpaque->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(1), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 1 * stride));

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_FRONT_BIT);
  CHECK_ERR(depthOpaque->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(2), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 2 * stride));

  Graphics::RenderState::PopDebugMarker();
  Graphics::RenderState::PushDebugMarker("Depth Masked Prepass");

  RenderState::SetShader(depthMasked);
  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);
  CHECK_ERR(depthMasked->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(3), StorageBuffer::read}));
  // CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 3 * stride));

  // Graphics::RenderState::SetCullMode(VK_CULL_MODE_BACK_BIT);
  // CHECK_ERR(depthMasked->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(4), StorageBuffer::read}));
  // CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 4 * stride));

  // Graphics::RenderState::SetCullMode(VK_CULL_MODE_FRONT_BIT);
  // CHECK_ERR(depthMasked->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(5), StorageBuffer::read}));
  // CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 5 * stride));

  Graphics::RenderState::PopDebugMarker();

  textures.Albedo = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Albedo)).GetTexture();
  textures.Normal = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Normal)).GetTexture();
  textures.Material = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Material)).GetTexture();
  textures.Emissive = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Emissive)).GetTexture();
  textures.Motion = CHECK_RES(Renderer::GlobalRenderTargetManager.GetRendertarget(context, rendertargets.Motion)).GetTexture();

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

  Graphics::RenderState::PushDebugMarker("Opaque Materials");
  CHECK_ERR(depthOpaque->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(0), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 0 * stride));

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_BACK_BIT);
  CHECK_ERR(depthOpaque->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(1), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 1 * stride));

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_FRONT_BIT);
  CHECK_ERR(depthOpaque->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(2), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 2 * stride));

  Graphics::RenderState::PopDebugMarker();
  Graphics::RenderState::PushDebugMarker("Masked Materials");

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);
  CHECK_ERR(depthMasked->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(3), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 3 * stride));

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_BACK_BIT);
  CHECK_ERR(depthMasked->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(4), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 4 * stride));

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_FRONT_BIT);
  CHECK_ERR(depthMasked->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(5), StorageBuffer::read}));
  CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 5 * stride));

  Graphics::RenderState::PopDebugMarker();

  RenderState::SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

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

  const auto stride = sizeof(VkDrawMeshTasksIndirectCommandEXT);

  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);
  // CHECK_ERR(forward->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(6), StorageBuffer::read}));
  // CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 6 * stride));

  // Graphics::RenderState::SetCullMode(VK_CULL_MODE_BACK_BIT);
  // CHECK_ERR(forward->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(7), StorageBuffer::read}));
  // CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 7 * stride));

  // Graphics::RenderState::SetCullMode(VK_CULL_MODE_FRONT_BIT);
  // CHECK_ERR(forward->Send(context, "DrawItems", {renderer.DrawBuffers.DrawLists.at(8), StorageBuffer::read}));
  // CHECK_ERR(DrawMeshTasksIndirect(context, renderer.DrawBuffers.DrawCommands, 8 * stride));

  RenderState::SetWindingOrder(VK_FRONT_FACE_CLOCKWISE);

  Graphics::RenderState::PopDebugMarker();

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

  AddGuiMethod<Engine::Physics::Rigidbody>(&Engine::Physics::Rigidbody::DrawGUI, true);

  AddGuiMethod<Engine::Physics::BoxShape>(&Engine::Physics::BoxShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::CylinderShape>(&Engine::Physics::CylinderShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::PlaneShape>(&Engine::Physics::PlaneShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::TaperedCylinderShape>(&Engine::Physics::TaperedCylinderShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::ConvexHullShape>(&Engine::Physics::ConvexHullShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::MeshShape>(&Engine::Physics::MeshShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::TaperedCapsuleShape>(&Engine::Physics::TaperedCapsuleShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::CapsuleShape>(&Engine::Physics::CapsuleShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::HeightFieldShape>(&Engine::Physics::HeightFieldShape::DrawGUI, true);
  AddGuiMethod<Engine::Physics::SphereShape>(&Engine::Physics::SphereShape::DrawGUI, true);

  PhysicsSystem.Init(Physics::MaxRigidbodies, 0UL, Physics::MaxBodyPairs, Physics::MaxContactConstraints, PhysicsBroadPhaseLayer, PhysicsObjectBroadphaseFilter,
                     PhysicsObjectObjectFilter);

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
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    lastUpdateResult = material.Update(*GetCurrentGraphicsContext());
  }));

  preRender.emplace_back(world.system<DirectionalLight>().kind(0).each([this, &buffers](flecs::entity entity, const DirectionalLight &light) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    lastUpdateResult = light.Write(buffers.DirectionalLightData, entity);
  }));

  preRender.emplace_back(world.system<PointLight>().kind(0).each([this, &buffers](flecs::entity entity, const PointLight &light) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    lastUpdateResult = light.Write(buffers.PointLightData, entity);
  }));

  preRender.emplace_back(world.system<SpotLight>().kind(0).each([this, &buffers](flecs::entity entity, const SpotLight &light) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    lastUpdateResult = light.Write(buffers.SpotLightData, entity);
  }));

  preRender.emplace_back(world.system<RectangleLight>().kind(0).each([this, &buffers](flecs::entity entity, const RectangleLight &light) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    lastUpdateResult = light.Write(buffers.RectangleLightData, entity);
  }));

  preRender.emplace_back(world.system<SphereLight>().kind(0).each([this, &buffers](flecs::entity entity, const SphereLight &light) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    lastUpdateResult = light.Write(buffers.SphereLightData, entity);
  }));

  preRender.emplace_back(world.system<Camera>().kind(0).each([this](flecs::entity entity, const Camera &camera) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    auto &transform = entity.get_mut<Transform>();
    auto &cameraMatrices = entity.get_mut<CameraMatrices>();

    lastUpdateResult = camera.WriteToBuffer(cameraMatrices, transform);
  }));

  preRender.emplace_back(
      world.system<Geometry, Renderer::Material>().kind(0).each([this](flecs::entity entity, Geometry &geometry, Renderer::Material &material) -> auto {
        [[unlikely]] if (lastUpdateResult.IsError()) { return; }

        auto &transform = entity.get_mut<Transform>();
        const auto &worldMatrix = transform.GetWorldMatrix();

        auto &renderer = Renderer::RendererInstance;

        if (geometry.mesh->GetBLAS() != nullptr) {
          bool isOpaque = material.alphaMode == Renderer::AlphaMode::Opaque;
          VkGeometryInstanceFlagsKHR flags = 0;

          flags |= isOpaque ? VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR : VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;

          if (!geometry.hasTlasIndex) {
            geometry.hasTlasIndex = true;

            auto result = renderer.GetSceneTLAS()->AddInstance(geometry.mesh->GetBLAS(), worldMatrix, flags);

            if (Error::IsError(result)) {
              lastUpdateResult = result.error();
              return;
            }

            geometry.tlasIndex = result.value();

            renderer.SetSceneNeedsTLASRebuild(true);
          } else {
            renderer.GetSceneTLAS()->UpdateInstanceTransform(geometry.tlasIndex, worldMatrix);
            renderer.GetSceneTLAS()->UpdateInstanceFlags(geometry.tlasIndex, flags);
          }
        }
      }));

  preRender.emplace_back(world.system<Geometry>().kind(0).each([this](flecs::entity entity, Geometry &geometry) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

    auto &renderer = Renderer::RendererInstance;

    [[unlikely]]
    if (geometry.dirty) {
      geometry.dirty = false;

      [[unlikely]]
      if (geometry.meshIndex == UINT32_MAX) {
        lastUpdateResult = Error::Create("Invalid mesh index");
        return;
      }

      const auto *material = entity.try_get<Renderer::Material>(entity);
      const auto *worldBounds = entity.try_get<WorldBounds>(entity);

      if (worldBounds == nullptr) {
        return;
      }

      auto drawRange = geometry.mesh->GetDrawRange();
      auto &context = *GetCurrentGraphicsContext();
      auto bufferAddressResult = geometry.mesh->GetVertexBuffer()->GetAddress(context, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      if (!bufferAddressResult.has_value()) {
        lastUpdateResult = bufferAddressResult.error();
      }

      [[unlikely]] if (lastUpdateResult.IsError()) { return; }

      Renderer::GPUMesh meshInfo{
          .Lod =
              Renderer::GPULOD{
                  .VerticesBuffer = Math::Uvec2(bufferAddressResult.value()),
                  .DrawRanges = {Math::Uvec2(drawRange.Offset, drawRange.Count)},
                  .LODRanges = {0.0},
                  .BoundsMin = Math::Vec4(worldBounds->Bounds.Min, 0.0),
                  .BoundsMax = Math::Vec4(worldBounds->Bounds.Max, 0.0),
              },
          .MaterialIndex = material != nullptr ? material->materialSSBOIndex : renderer.NoMaterial.materialSSBOIndex,
          .TransformIndex = geometry.transformIndex,
      };

      auto span = std::span<Renderer::GPUMesh>(&meshInfo, 1);

      lastUpdateResult = renderer.MeshBuffer->GetBuffer()->SetData(context, span, geometry.meshIndex * renderer.MeshAllocator.itemSize);
      [[unlikely]] if (lastUpdateResult.IsError()) { return; }
    }
  }));

  preRender.emplace_back(world.system<Volume>().kind(0).each([this](flecs::entity entity, Volume &volume) -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

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

    [[unlikely]] if (lastUpdateResult.IsError()) { return; }
  }));

  finalizePreRenderUploads = world.system().kind(0).each([this, &buffers]() -> auto {
    [[unlikely]] if (lastUpdateResult.IsError()) { return; }

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
  Renderer::RendererInstance.GetLineDrawer().NewFrame();
  Renderer::RendererInstance.GetPrimitiveDrawer().NewFrame();

  world.progress(static_cast<float>(deltaTime));
  Renderer::GlobalRenderTargetManager.Update();
  DeferredManager::Instance.Update();

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