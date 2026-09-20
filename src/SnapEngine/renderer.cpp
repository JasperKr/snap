#include "renderer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include "Modules/imageData.hpp"
#include "Renderer/rendertargetManager.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/Lights/directionalLight.hpp"
#include "Scene/Lights/pointLight.hpp"
#include "Scene/Lights/rectangleLight.hpp"
#include "Scene/Lights/sphereLight.hpp"
#include "Scene/Lights/spotLight.hpp"
#include "bufferUploadManager.hpp"
#include "material.hpp"
#include <cassert>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
Renderer RendererInstance{};

auto Renderer::InitializeDefaultMaterial(Graphics::GraphicsContext &context)
    -> Error {
  constexpr uint32_t CheckerboardTextureSize = 8;
  constexpr Color CheckerboardColorPrimary = {0.0F, 0.0F, 0.0F, 1.0F};
  constexpr Color CheckerboardColorSecondary = {1.0F, 0.0F, 1.0F, 1.0F};

  Image::ImageData defaultTextureData(
      {CheckerboardTextureSize, CheckerboardTextureSize, 1},
      VK_FORMAT_R8G8B8A8_UNORM);

  for (uint32_t column = 0; column < CheckerboardTextureSize; ++column) {
    for (uint32_t row = 0; row < CheckerboardTextureSize; ++row) {
      bool isPrimaryColor = (row % 2 == 0 && column % 2 == 0) ||
                            (row % 2 == 1 && column % 2 == 1);

      Color pixelColor = isPrimaryColor ? CheckerboardColorPrimary
                                        : CheckerboardColorSecondary;

      CHECK_ERR(defaultTextureData.SetColor({column, row, 0}, pixelColor));
    }
  }

  auto defaultTextureResult = Graphics::Texture::FromMemory(
      context, defaultTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);

  if (Error::IsError(defaultTextureResult)) {
    return defaultTextureResult.error();
  }

  Image::ImageData defaultNormalTextureData({1, 1, 1},
                                            VK_FORMAT_R8G8B8A8_UNORM);

  const Color pixelColor = {0.5F, 0.5F, 1.0F, 1.0F};
  CHECK_ERR(defaultNormalTextureData.SetColor({}, pixelColor));

  auto defaultNormalTextureResult = Graphics::Texture::FromMemory(
      context, defaultNormalTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);

  if (Error::IsError(defaultNormalTextureResult)) {
    return defaultNormalTextureResult.error();
  }
  auto defaultNormalTexture = defaultNormalTextureResult.value();

  Image::ImageData defaultBlackTextureData({1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM);
  const Color blackColor = {0.0F, 0.0F, 0.0F, 1.0F};
  CHECK_ERR(defaultBlackTextureData.SetColor({}, blackColor));

  auto defaultBlackTextureResult = Graphics::Texture::FromMemory(
      context, defaultBlackTextureData, VK_IMAGE_USAGE_SAMPLED_BIT);
  if (Error::IsError(defaultBlackTextureResult)) {
    return defaultBlackTextureResult.error();
  }
  auto defaultBlackTexture = defaultBlackTextureResult.value();

  auto defaultWhiteTexture = CHECK_RES(Graphics::Texture::GetDefault(
      context, VK_FORMAT_R8G8B8A8_UNORM, Graphics::TextureType::DEFAULT));

  auto material = Material();

  material.name = "No Material";
  material.albedoTexture = defaultTextureResult.value();
  material.albedoTexture->SetFilter(VK_FILTER_LINEAR, VK_FILTER_NEAREST,
                                    VK_SAMPLER_MIPMAP_MODE_NEAREST);
  material.metallicRoughnessTexture = material.albedoTexture;
  material.emissiveTexture = defaultWhiteTexture;
  material.normalTexture = defaultNormalTexture;
  material.reflectanceTexture = defaultWhiteTexture;

  material.cullMode = VK_CULL_MODE_NONE;
  material.alphaMode = AlphaMode::Opaque;

  NoMaterial = material;

  material = Material();
  material.name = "Default Material";
  material.albedoTexture = defaultWhiteTexture;
  material.metallicRoughnessTexture = defaultWhiteTexture;
  material.emissiveTexture = defaultWhiteTexture;
  material.normalTexture = defaultNormalTexture;
  material.reflectanceTexture = defaultWhiteTexture;

  material.cullMode = VK_CULL_MODE_BACK_BIT;
  material.alphaMode = AlphaMode::Opaque;

  DefaultMaterial = material;

  return {};
}

auto Renderer::GetNewMaterialIndex() -> Result<size_t> {
  size_t newIndex = 0;
  while (UsedMaterialIndices.contains(newIndex)) {
    newIndex++;
  }
  UsedMaterialIndices.insert(newIndex);

  if (newIndex >= MaterialsBuffer->GetElementCount()) {
    auto newElementCount = MaterialsBuffer->GetElementCount() * 2;

    MaterialsBuffer = CHECK_RES(MaterialsBuffer->Grow(
        *Graphics::GetCurrentGraphicsContext(), newElementCount));

    CHECK_ERR(MaterialUploadManager.SetNewBuffer(MaterialsBuffer->GetBuffer()));
  }

  return newIndex;
}

auto Renderer::AssureModelTransformBufferSize(size_t minimumSize) -> Error {
  if (minimumSize >= ModelTransformsBuffer->GetElementCount()) {
    auto newElementCount = ModelTransformsBuffer->GetElementCount() * 2;

    ModelTransformsBuffer = CHECK_RES(ModelTransformsBuffer->Grow(
        *Graphics::GetCurrentGraphicsContext(), newElementCount));
  }

  return {};
}

auto Renderer::InitializeMaterialBuffer(Graphics::GraphicsContext &context,
                                        size_t initialSize) -> Error {
  const Graphics::StructuredBufferCreationInfo materialBufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Material Buffer",
  };

  auto format = Graphics::BufferFormat(MaterialBufferComponents);

  MaterialsBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(
      context, format, initialSize, materialBufferCreateInfo));
  MaterialUploadManager = BufferUploadManager(MaterialsBuffer->GetBuffer(),
                                              MaterialsBuffer->GetStride());

  return {};
}

auto Renderer::InitializeLightBuffers(Graphics::GraphicsContext &context)
    -> Error {
  Graphics::StructuredBufferCreationInfo bufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags =
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Directional Light Buffer",
  };

  SceneLightBuffers.DirectionalLightsBuffer =
      CHECK_RES(Graphics::StructuredBuffer::Create(
          context, DirectionalLight::GetBufferFormat(), MaxDirectionalLights,
          bufferCreateInfo));

  bufferCreateInfo.debugName = "Point Light Buffer";
  SceneLightBuffers.PointLightsBuffer = CHECK_RES(
      Graphics::StructuredBuffer::Create(context, PointLight::GetBufferFormat(),
                                         MaxPointLights, bufferCreateInfo));

  bufferCreateInfo.debugName = "Spot Light Buffer";
  SceneLightBuffers.SpotLightsBuffer = CHECK_RES(
      Graphics::StructuredBuffer::Create(context, SpotLight::GetBufferFormat(),
                                         MaxSpotLights, bufferCreateInfo));

  bufferCreateInfo.debugName = "Rectangle Light Buffer";
  SceneLightBuffers.RectangleLightsBuffer =
      CHECK_RES(Graphics::StructuredBuffer::Create(
          context, RectangleLight::GetBufferFormat(), MaxRectangleLights,
          bufferCreateInfo));

  bufferCreateInfo.debugName = "Sphere Light Buffer";
  SceneLightBuffers.SphereLightsBuffer =
      CHECK_RES(Graphics::StructuredBuffer::Create(
          context, SphereLight::GetBufferFormat(), MaxSphereLights,
          bufferCreateInfo));

  SceneLightBuffers.DirectionalLightData.resize(
      MaxDirectionalLights *
      SceneLightBuffers.DirectionalLightsBuffer->GetStride());
  SceneLightBuffers.PointLightData.resize(
      MaxPointLights * SceneLightBuffers.PointLightsBuffer->GetStride());
  SceneLightBuffers.SpotLightData.resize(
      MaxSpotLights * SceneLightBuffers.SpotLightsBuffer->GetStride());
  SceneLightBuffers.RectangleLightData.resize(
      MaxRectangleLights *
      SceneLightBuffers.RectangleLightsBuffer->GetStride());
  SceneLightBuffers.SphereLightData.resize(
      MaxSphereLights * SceneLightBuffers.SphereLightsBuffer->GetStride());

  return {};
}

auto Renderer::InitializeModelTransformsBuffer(
    Graphics::GraphicsContext &context, size_t initialSize) -> Error {
  const Graphics::StructuredBufferCreationInfo bufferCreateInfo{
      .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .usageFlags =
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .debugName = "Model Transforms Buffer",
  };

  auto format = Graphics::BufferFormat(ModelTransformBufferComponents);

  ModelTransformsBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(
      context, format, initialSize, bufferCreateInfo));

  return {};
}

auto Renderer::BindLightBuffers(const Graphics::GraphicsContext &context,
                                const Ref<Graphics::Shader> &shader) const
    -> Error {
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

  key = Key{"DirectionalLights"};
  auto error = shader->Send(key, SceneLightBuffers.DirectionalLightsBuffer);
  if (Error::IsError(error)) {
    return error;
  }

  return {};
}

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error {
  return Graphics::Draw(context, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                        3, // NOLINT
                        1);
}

void Renderer::Deinitialize() {
  GlobalRenderTargetManager.Deinitialize();
  PrefilterManager.Deinitialize();
  LineDrawer.Deinitialize();
  PrimitiveDrawer.Deinitialize();
  BloomManager.Deinitialize();

  if (!initialized) {
    return;
  }

  NoMaterial = Material();
  DefaultMaterial = Material();
  MaterialsBuffer.reset();
  ModelTransformsBuffer.reset();

  SceneLightBuffers = Lights();
  LightProbeBuffer = nullptr;
  ShaderManager = ::Engine::Renderer::ShaderManager();
  SceneTLAS = nullptr;

  BlueNoiseTexture = nullptr;
  PMJ02bnSamples = nullptr;
}

} // namespace Engine::Renderer