#include "renderer.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/imageData.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "bufferUploadManager.hpp"
#include "defer.hpp"
#include "material.hpp"
#include <cassert>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Renderer RendererInstance;

auto Renderer::InitializeDefaultMaterial(Graphics::GraphicsContext &context) -> Error {
  constexpr uint32_t CheckerboardTextureSize = 8;
  constexpr Color CheckerboardColorPrimary = {0.0F, 0.0F, 0.0F, 1.0F};
  constexpr Color CheckerboardColorSecondary = {1.0F, 0.0F, 1.0F, 1.0F};

  constexpr Color CheckerboardMaterialPrimary = {0.0F, 1.0F, 0.0F, 0.0F};
  constexpr Color CheckerboardMaterialSecondary = {0.0F, 0.0F, 1.0F, 0.0F};

  Image::ImageData defaultTextureData({CheckerboardTextureSize, CheckerboardTextureSize, 1}, VK_FORMAT_R8G8B8A8_UNORM);
  Image::ImageData defaultMaterialData({CheckerboardTextureSize, CheckerboardTextureSize, 1}, VK_FORMAT_R8G8B8A8_UNORM);

  for (uint32_t column = 0; column < CheckerboardTextureSize; ++column) {
    for (uint32_t row = 0; row < CheckerboardTextureSize; ++row) {
      bool isPrimaryColor = (row % 2 == 0 && column % 2 == 0) || (row % 2 == 1 && column % 2 == 1);

      Color pixelColor = isPrimaryColor ? CheckerboardColorPrimary : CheckerboardColorSecondary;
      Color pixelMaterial = isPrimaryColor ? CheckerboardMaterialPrimary : CheckerboardMaterialSecondary;

      CHECK_ERR(defaultTextureData.SetColor({column, row, 0}, pixelColor));
      CHECK_ERR(defaultMaterialData.SetColor({column, row, 0}, pixelMaterial));
    }
  }

  auto defaultTexture = CHECK_RES(Graphics::Texture::FromMemory(context, defaultTextureData, VK_IMAGE_USAGE_SAMPLED_BIT));
  auto materialTexture = CHECK_RES(Graphics::Texture::FromMemory(context, defaultMaterialData, VK_IMAGE_USAGE_SAMPLED_BIT));

  NoMaterial = Material();

  NoMaterial.name = "No Material";
  NoMaterial.albedoTexture = defaultTexture;
  NoMaterial.albedoTexture->SetFilter(VK_FILTER_LINEAR, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
  NoMaterial.metallicRoughnessTexture = materialTexture;
  NoMaterial.metallicRoughnessTexture->SetFilter(VK_FILTER_LINEAR, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
  NoMaterial.cullMode = VK_CULL_MODE_NONE;
  NoMaterial.alphaMode = AlphaMode::Opaque;

  DefaultMaterial = Material();
  DefaultMaterial.name = "Default Material";
  DefaultMaterial.cullMode = VK_CULL_MODE_BACK_BIT;
  DefaultMaterial.alphaMode = AlphaMode::Opaque;

  return {};
}

auto Renderer::GetNewMaterialIndex() -> Result<size_t> {
  size_t newIndex = CHECK_RES(MaterialAllocator.Allocate());

  if (newIndex >= MaterialsBuffer->GetElementCount()) {
    auto newElementCount = MaterialsBuffer->GetElementCount() * 2;

    MaterialsBuffer = CHECK_RES(MaterialsBuffer->Grow(*Graphics::GetCurrentGraphicsContext(), newElementCount));

    CHECK_ERR(MaterialUploadManager.SetNewBuffer(MaterialsBuffer->GetBuffer()));
  }

  return newIndex;
}

auto Renderer::AssureModelTransformBufferSize(size_t minimumSize) -> Error {
  if (minimumSize >= ModelTransformsBuffer->GetElementCount()) {
    auto newElementCount = ModelTransformsBuffer->GetElementCount() * 2;

    ModelTransformsBuffer = CHECK_RES(ModelTransformsBuffer->Grow(*Graphics::GetCurrentGraphicsContext(), newElementCount));
  }

  return {};
}

auto Renderer::InitializeMaterialBuffer(const Graphics::GraphicsContext &context, size_t initialSize) -> Error {
  const Graphics::StructuredBufferCreationInfo materialBufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Material Buffer",
  };

  auto format = Graphics::BufferFormat(MaterialBufferComponents);

  MaterialsBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(context, format, initialSize, materialBufferCreateInfo));
  MaterialUploadManager = BufferUploadManager(MaterialsBuffer->GetBuffer(), MaterialsBuffer->GetStride());

  return {};
}

auto Renderer::InitializeMeshBuffer(const Graphics::GraphicsContext &context, size_t initialSize) -> Error {
  const Graphics::StructuredBufferCreationInfo meshBufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Mesh Buffer",
  };

  auto format = Graphics::BufferFormat(MeshBufferComponents);

  MeshBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(context, format, initialSize, meshBufferCreateInfo));

  return {};
}

auto Renderer::InitializeLightBuffers(const Graphics::GraphicsContext &context) -> Error {
  Graphics::StructuredBufferCreationInfo bufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Directional Light Buffer",
  };

  SceneLightBuffers.DirectionalLightsBuffer =
      CHECK_RES(Graphics::StructuredBuffer::Create(context, DirectionalLight::GetBufferFormat(), MaxDirectionalLights, bufferCreateInfo));

  bufferCreateInfo.debugName = "Point Light Buffer";
  SceneLightBuffers.PointLightsBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(context, PointLight::GetBufferFormat(), MaxPointLights, bufferCreateInfo));

  bufferCreateInfo.debugName = "Spot Light Buffer";
  SceneLightBuffers.SpotLightsBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(context, SpotLight::GetBufferFormat(), MaxSpotLights, bufferCreateInfo));

  bufferCreateInfo.debugName = "Rectangle Light Buffer";
  SceneLightBuffers.RectangleLightsBuffer =
      CHECK_RES(Graphics::StructuredBuffer::Create(context, RectangleLight::GetBufferFormat(), MaxRectangleLights, bufferCreateInfo));

  bufferCreateInfo.debugName = "Sphere Light Buffer";
  SceneLightBuffers.SphereLightsBuffer =
      CHECK_RES(Graphics::StructuredBuffer::Create(context, SphereLight::GetBufferFormat(), MaxSphereLights, bufferCreateInfo));

  SceneLightBuffers.DirectionalLightData.resize(MaxDirectionalLights * SceneLightBuffers.DirectionalLightsBuffer->GetStride());
  SceneLightBuffers.PointLightData.resize(MaxPointLights * SceneLightBuffers.PointLightsBuffer->GetStride());
  SceneLightBuffers.SpotLightData.resize(MaxSpotLights * SceneLightBuffers.SpotLightsBuffer->GetStride());
  SceneLightBuffers.RectangleLightData.resize(MaxRectangleLights * SceneLightBuffers.RectangleLightsBuffer->GetStride());
  SceneLightBuffers.SphereLightData.resize(MaxSphereLights * SceneLightBuffers.SphereLightsBuffer->GetStride());

  return {};
}

auto Renderer::InitializeModelTransformsBuffer(const Graphics::GraphicsContext &context, size_t initialSize) -> Error {
  const Graphics::StructuredBufferCreationInfo bufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Model Transforms Buffer",
  };

  auto format = Graphics::BufferFormat(ModelTransformBufferComponents);

  ModelTransformsBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(context, format, initialSize, bufferCreateInfo));

  return {};
}

auto Renderer::BindLightBuffers(const Graphics::GraphicsContext &context, const Ref<Graphics::Shader> &shader) const -> Error {
  using Key = Graphics::ResourceKey;

  static auto key = Key{"PointLights"};
  // auto error = shader->Send(key, SceneLightBuffers.PointLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  // key = Key{"SpotLights"};
  // error = shader->Send(key, SceneLightBuffers.SpotLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  // key = Key{"RectangleLights"};
  // error = shader->Send(key, SceneLightBuffers.RectangleLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  // key = Key{"SphereLights"};
  // error = shader->Send(key, SceneLightBuffers.SphereLightsBuffer);
  // if (Error::IsError(error)) {
  //   return error;
  // }

  CHECK_ERR(shader->Send(context, "DirectionalLights", {SceneLightBuffers.DirectionalLightsBuffer, Graphics::StorageBuffer::read}));

  return {};
}

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error {
  return Graphics::Draw(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                        3, // NOLINT
                        1);
}

void Renderer::Deinitialize() {
  DeferredManager::Instance.Shutdown();

  GlobalRenderTargetManager.Deinitialize();
  PrefilterManager.Deinitialize();
  LineDrawer.Deinitialize();
  PrimitiveDrawer.Deinitialize();
  BloomManager.Deinitialize();
  VolumeManager = {};

  if (!initialized) {
    return;
  }

  initialized = false;

  NoMaterial = Material();
  DefaultMaterial = Material();
  MaterialsBuffer.reset();
  MaterialUploadManager = {};
  ModelTransformsBuffer.reset();
  MeshBuffer.reset();

  SceneLightBuffers = Lights();
  LightProbeBuffer = nullptr;
  ShaderManager = ::Engine::Renderer::ShaderManager();
  SceneTLAS = nullptr;

  BlueNoiseTexture = nullptr;
  PMJ02bnSamples = nullptr;
  // BLASPool.DeInitialize(); Deletes the pool before the objects, causes issues.
  // It is valid never to destroy the pool as long as all objects are removed, no references will dangle.
}

auto Renderer::NewFrame(const Graphics::GraphicsContext &context) -> Error {
  CHECK_ERR(NoMaterial.Update(context));
  CHECK_ERR(DefaultMaterial.Update(context));

  return {};
}
auto Renderer::Initialize(Graphics::GraphicsContext &context) -> Error {
  const size_t InitialMaterialBufferSize = 1024UL;
  const size_t InitialModelTransformBufferSize = 4096UL;
  const size_t InitialMeshBufferSize = 4096UL;

  CHECK_ERR(InitializeDefaultMaterial(context));

  CHECK_ERR(InitializeMaterialBuffer(context, InitialMaterialBufferSize));
  CHECK_ERR(InitializeModelTransformsBuffer(context, InitialModelTransformBufferSize));
  CHECK_ERR(InitializeMeshBuffer(context, InitialMeshBufferSize));

  CHECK_ERR(MaterialAllocator.Resize(InitialMaterialBufferSize * MaterialsBuffer->GetStride()));
  CHECK_ERR(TransformAllocator.Resize(InitialModelTransformBufferSize * ModelTransformsBuffer->GetStride()));

  PrintInfo("Stride: {}", MeshBuffer->GetStride());
  CHECK_ERR(MeshAllocator.Resize(InitialMeshBufferSize * MeshBuffer->GetStride()));

  CHECK_ERR(InitializeLightBuffers(context));
  CHECK_ERR(PrefilterManager.Initialize(context));

  LightProbeBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(context, LightProbeBufferFormat, 64UL,
                                                                  Graphics::StructuredBufferCreationInfo{
                                                                      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                      .usageFlags = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
                                                                                    static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                                                                                    static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
                                                                      .debugName = "Light Probe Buffer",
                                                                  }));

  CHECK_ERR(LineDrawer.Initialize(context));
  CHECK_ERR(PrimitiveDrawer.Initialize(context));
  CHECK_ERR(BloomManager.Initialize(context));
  SceneTLAS = CHECK_RES(Graphics::TLAS::Create(context));
  BlueNoiseTexture = CHECK_RES(Graphics::Texture::FromFile(context, "src/Scripting/Graphics/Assets/blue_noise_128.bmp", VK_IMAGE_USAGE_SAMPLED_BIT,
                                                           Graphics::TextureMipmapOption::None));
  BlueNoiseTexture->SetWrapmode(VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);

  auto samples = CHECK_RES(Filesystem::ReadFile("src/Snap/Graphics/Assets/pmj_samples.bin"));
  Graphics::BufferCreationInfo info = {
      .size = samples.size(),
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .stagingBuffer = false,
      .persistentMapping = false,
      .debugName = "Progressive Multi-Jittered (0,2) Blue Noise Samples",
  };

  PMJ02bnSamples = CHECK_RES(Graphics::Buffer::Create(context, info));
  CHECK_ERR(PMJ02bnSamples->SetData(context, samples))

  CHECK_ERR(ShaderManager.Preload());
  BLASPool = {};
  CHECK_ERR(VolumeManager.Initialize(context));

  for (int i = 0; i < DrawBuffers.DrawCommands.size(); i++) {
    Graphics::BufferCreationInfo drawCommandInfo = {
        .size = sizeof(VkDrawIndirectCommand),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .stagingBuffer = false,
        .persistentMapping = false,
        .debugName = "Draw commands",
    };

    DrawBuffers.DrawCommands.at(i) = CHECK_RES(Graphics::Buffer::Create(context, drawCommandInfo));

    Graphics::BufferCreationInfo drawListsInfo = {
        .size = MaxDrawsPerPass * sizeof(uint32_t) * 8,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .stagingBuffer = false,
        .persistentMapping = false,
        .debugName = "Draw lists",
    };

    DrawBuffers.DrawLists.at(i) = CHECK_RES(Graphics::Buffer::Create(context, drawListsInfo));
  }

  Graphics::BufferCreationInfo drawCountsInfo = {
      .size = sizeof(uint32_t) * 9,
      .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .stagingBuffer = false,
      .persistentMapping = false,
      .debugName = "Draw command counts",
  };

  DrawBuffers.DrawIndices = CHECK_RES(Graphics::Buffer::Create(context, drawCountsInfo));

  initialized = true;
  return {};
}
} // namespace Engine::Renderer