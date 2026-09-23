#include "prefilterManager.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Renderer/lightProbe.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/camera.hpp"
#include "Scene/cameraMatrices.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "renderer.hpp"
#include <array>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

auto LightprobePrefilterManager::Initialize( // NOLINT
    const Graphics::GraphicsContext &context) -> Error {
  PrefilteredRadianceCoeffs =
      CHECK_RES(Filesystem::ReadFile("Graphics/Assets/coeffs_quad_32.bin"));

  PrefilteredRadianceCoeffsBuffer = CHECK_RES(Graphics::Buffer::Create(
      context,
      {
          .size = PrefilteredRadianceCoeffs.size(),
          .usage = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT),
          .properties =
              static_cast<uint32_t>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
          .debugName = "Prefiltered Radiance Coefficients Buffer",
      }));

  CHECK_ERR(PrefilteredRadianceCoeffsBuffer->SetData(
      context, PrefilteredRadianceCoeffs));

  SceneCubemap = CHECK_RES(Graphics::Texture::Create(
      context, {
                   .size = {PrefilteredRadianceCubeMapSize,
                            PrefilteredRadianceCubeMapSize, 1},
                   .arrayLayers = 6,
                   .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                   .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                   .mipmapCount = ProbeReflectionMipLevels,
                   .debugName = "Scene Cubemap",
                   .textureType = Graphics::TextureType::CUBEMAP,
               }));
  SceneCubemap->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                          VK_SAMPLER_MIPMAP_MODE_LINEAR);

  for (uint32_t layer = 0; layer < 6; layer++) { // NOLINT
    auto inputArrayView = CHECK_RES(Graphics::Texture::Create(
        context, SceneCubemap.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = layer,
            .layerCount = 1,
        }));
    EnvMapArrayViews.emplace_back(inputArrayView);
  }

  TemporaryRadianceMap = CHECK_RES(Graphics::Texture::Create(
      context, {
                   .size = {PrefilteredRadianceCubeMapSize,
                            PrefilteredRadianceCubeMapSize, 1},
                   .arrayLayers = 6,
                   .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                   .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_STORAGE_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                   .mipmapCount = ProbeReflectionMipLevels,
                   .debugName = "Temporary Radiance Map",
                   .textureType = Graphics::TextureType::CUBEMAP,
               }));

  TemporaryRadianceMap->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                                  VK_SAMPLER_MIPMAP_MODE_LINEAR);

  constexpr static float EnvironmentMapLevel0PB = 6.0F;
  constexpr static float EnvironmentMapLevel1PB = 0.0F;

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    uint32_t levelSize = PrefilteredRadianceCubeMapSize >> level;

    float glossiness = (EnvironmentMapLevel0PB - static_cast<float>(level)) /
                       (EnvironmentMapLevel0PB - EnvironmentMapLevel1PB);

    levelParameters.at(level) = {
        .Level = level,
        .Glossiness = glossiness,
        .Roughness = GlossinessToRoughness(glossiness),
        .Resolution = levelSize,
    };
  }

  CHECK_ERR(CreateStorageTextures(context, EnvMapReallocationStep));

  // NOLINTNEXTLINE
  Camera = CHECK_RES(Engine::Camera::Create(
      context, 90.0F,
      {PrefilteredRadianceCubeMapSize, PrefilteredRadianceCubeMapSize}, 0.1F,
      100.0F));
  Camera.SetPersistentTextureSettings({
      .IncomingLight = true,
  });
  Camera.SetSettings(Camera::Settings{
      .DoPostProcessing = false,
  });

  return {};
}

auto LightprobePrefilterManager::CreateStorageTextures(
    const Graphics::GraphicsContext &context, uint32_t arrayLayers) -> Error {
  RadianceMapViews.clear();
  IrradianceMapViews.clear();

  RadianceMaps = CHECK_RES(Graphics::Texture::Create(
      context,
      Graphics::TextureCreationInfo{
          .size = {PrefilteredRadianceMapsSize, PrefilteredRadianceMapsSize, 1},
          .arrayLayers = arrayLayers,
          .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
          .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                   VK_IMAGE_USAGE_STORAGE_BIT,
          .mipmapCount = ProbeReflectionMipLevels,
          .debugName = "Radiance Maps",
          .textureType = Graphics::TextureType::ARRAY,
      }));

  for (uint32_t level = 0; level < RadianceMaps->GetMipmapCount(); level++) {
    auto view = CHECK_RES(Graphics::Texture::Create(
        context, RadianceMaps.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = arrayLayers,
        }));
    RadianceMapViews.emplace_back(view);
  }

  IrradianceMaps = CHECK_RES(Graphics::Texture::Create(
      context, Graphics::TextureCreationInfo{
                   .size = {PrefilteredIrradianceMapsSize,
                            PrefilteredIrradianceMapsSize, 1},
                   .arrayLayers = arrayLayers,
                   .format = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                   .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_STORAGE_BIT,
                   .mipmapCount = 1,
                   .debugName = "Irradiance Maps",
                   .textureType = Graphics::TextureType::ARRAY,
               }));

  for (uint32_t level = 0; level < IrradianceMaps->GetMipmapCount(); level++) {
    auto view = CHECK_RES(Graphics::Texture::Create(
        context, IrradianceMaps.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = arrayLayers,
        }));
    IrradianceMapViews.emplace_back(view);
  }

  return {};
}

// NOLINTNEXTLINE
auto LightprobePrefilterManager::PrefilterRadianceMap(
    const Graphics::GraphicsContext &context,
    const Ref<Graphics::Texture> &envMap, const Ref<Graphics::Texture> &output,
    const LightProbe &lightProbe) -> Error {

  if (envMap->GetFormat() != VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
    return Error::Create("Environment map must be in rg11b10f format "
                         "for prefiltering.");
  }

  if (envMap->GetWidth() != PrefilteredRadianceCubeMapSize ||
      envMap->GetHeight() != PrefilteredRadianceCubeMapSize) {
    return Error::Createf("Environment map must be {}x{} for prefiltering.",
                          PrefilteredRadianceCubeMapSize,
                          PrefilteredRadianceCubeMapSize);
  }

  if (!envMap->SupportsSampling() || !envMap->SupportsStorage()) {
    return Error::Create("Environment map must support sampling and storage "
                         "for prefiltering.");
  }

  envMap->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                    VK_SAMPLER_MIPMAP_MODE_LINEAR);
  envMap->SetAnisotropy(0.0F); // NOLINT
  output->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                    VK_SAMPLER_MIPMAP_MODE_LINEAR);
  output->SetAnisotropy(0.0F); // NOLINT

  /// Prepare texture views ///

  std::vector<Ref<Graphics::Texture>> envMapCubeViews;
  std::vector<Ref<Graphics::Texture>> envMapArrayViews;
  std::vector<Ref<Graphics::Texture>> outputViews;

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    auto inputCubeView = CHECK_RES(Graphics::Texture::Create(
        context, envMap.get(), VK_IMAGE_VIEW_TYPE_CUBE,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6,
        }));
    envMapCubeViews.emplace_back(inputCubeView);

    auto inputArrayView = CHECK_RES(Graphics::Texture::Create(
        context, envMap.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6,
        }));
    envMapArrayViews.emplace_back(inputArrayView);

    auto outputView = CHECK_RES(Graphics::Texture::Create(
        context, output.get(), VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        VkImageSubresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = level,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6,
        }));
    outputViews.emplace_back(outputView);
  }

  /// Run the downsample shader to generate the mipmaps for the environment map ///

  auto downsampleShader =
      CHECK_RES(RendererInstance.GetShaderManager().GetShader(
          ShaderKey::DownsampleEnvironmentMap));

  Graphics::RenderState::SetShader(downsampleShader);

  for (uint32_t level = 0; level < ProbeReflectionMipLevels - 1; level++) {
    auto &params = levelParameters.at(level);

    CHECK_ERR(
        downsampleShader->Send({"tex_hi_res"}, envMapCubeViews.at(level)));
    CHECK_ERR(
        downsampleShader->Send({"tex_lo_res"}, envMapArrayViews.at(level + 1)));

    CHECK_ERR(Graphics::DispatchWithin(
        context, {params.Resolution, params.Resolution, 6}));
  }

  // Prefilter the environment map for radiance ///
  auto prefilterRadianceShader =
      CHECK_RES(RendererInstance.GetShaderManager().GetShader(
          ShaderKey::PrefilterRadiance));

  CHECK_ERR(prefilterRadianceShader->Send({"Coefficients"},
                                          PrefilteredRadianceCoeffsBuffer));
  CHECK_ERR(prefilterRadianceShader->Send({"InputTexture"}, envMap));

  Graphics::RenderState::SetShader(prefilterRadianceShader);

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    auto key = std::format("OutputTexture{}", level);
    const auto &view = outputViews.at(level);

    CHECK_ERR(prefilterRadianceShader->Send({key.c_str()}, view));
  }

  constexpr static uint64_t PixelCount = Image::GetTexelCount(
      VkExtent2D{
          .width = PrefilteredRadianceCubeMapSize,
          .height = PrefilteredRadianceCubeMapSize,
      },
      ProbeReflectionMipLevels);

  CHECK_ERR(Graphics::DispatchWithin(context, {PixelCount, 6, 1}));

  /// Store the environment map in octahedral format for sampling in the shader ///
  auto storeEnvironmentMapShader =
      CHECK_RES(RendererInstance.GetShaderManager().GetShader(
          ShaderKey::StoreEnvironmentMap));

  CHECK_ERR(storeEnvironmentMapShader->Send({"Input"}, TemporaryRadianceMap));

  for (uint32_t level = 0; level < ProbeReflectionMipLevels; level++) {
    auto key = std::format("Output_Mip_{}", level);
    const auto &view = RadianceMapViews.at(level);

    CHECK_ERR(storeEnvironmentMapShader->Send({key.c_str()}, view));
  }

  CHECK_ERR(Graphics::UniformWriter::Send(storeEnvironmentMapShader, {"Slice"},
                                          lightProbe.EnvironmentMapIndex));

  Graphics::RenderState::SetShader(storeEnvironmentMapShader);

  constexpr static uint64_t FullPixelCount = Image::GetTexelCount(
      VkExtent2D{
          .width = PrefilteredRadianceMapsSize,
          .height = PrefilteredRadianceMapsSize,
      },
      ProbeReflectionMipLevels);

  CHECK_ERR(Graphics::DispatchWithin(context, {FullPixelCount, 1, 1}));

  return {};
}

auto LightprobePrefilterManager::PrefilterIrradianceMap(
    const Graphics::GraphicsContext &context, int32_t slice) -> Error {
  ERR_ASSERT(slice >= 0);

  auto prefilterIrradianceShader =
      CHECK_RES(RendererInstance.GetShaderManager().GetShader(
          ShaderKey::PrefilterIrradiance));

  CHECK_ERR(Graphics::UniformWriter::Send(prefilterIrradianceShader, {"Slice"},
                                          slice));
  CHECK_ERR(prefilterIrradianceShader->Send({"RadianceInput"}, RadianceMaps));
  CHECK_ERR(
      prefilterIrradianceShader->Send({"IrradianceOutput"}, IrradianceMaps));

  // NOLINTBEGIN
  if (IrradianceMaps->GetWidth() != 32UL ||
      IrradianceMaps->GetHeight() != 32UL) {
    return Error::Create("Irradiance map must be 32x32 for prefiltering.");
  }
  // NOLINTEND

  Graphics::RenderState::SetShader(prefilterIrradianceShader);

  CHECK_ERR(
      Graphics::DispatchWithin(context, {PrefilteredIrradianceMapsSize,
                                         PrefilteredIrradianceMapsSize, 1}));

  return {};
}

auto LightprobePrefilterManager::PrefilterEnvironmentMap(
    const Graphics::GraphicsContext &context,
    const Ref<Graphics::Texture> &envMap, LightProbe &lightProbe) -> Error {
  if (lightProbe.EnvironmentMapIndex < 0) {
    return Error::Create("Invalid environment map index");
  }

  if (static_cast<uint32_t>(lightProbe.EnvironmentMapIndex) >= MaxEnvMaps) {
    return Error::Create("Environment map index out of bounds");
  }

  if (!EnvMapUsed.at(lightProbe.EnvironmentMapIndex) &&
      lightProbe.EnvironmentMapIndex != 0) {
    return Error::Create("Environment map index not in use");
  }

  CHECK_ERR(
      PrefilterRadianceMap(context, envMap, TemporaryRadianceMap, lightProbe));

  CHECK_ERR(PrefilterIrradianceMap(context, lightProbe.EnvironmentMapIndex));

  return {};
}

auto CreateRotated(float yaw, float pitch, float roll) -> Engine::Transform {
  return Engine::Transform(Math::Conversions::ToQuaternion(
      Math::EulerAngle(pitch, yaw, roll).ToRadians()));
}

const static std::array<Engine::Transform, 6> Transforms = {
    CreateRotated(90.0F, 0.0F, 0.0F),  // X+
    CreateRotated(-90.0F, 0.0F, 0.0F), // X-
    CreateRotated(0.0F, -90.0F, 0.0F), // Y+
    CreateRotated(0.0F, 90.0F, 0.0F),  // Y-
    CreateRotated(0.0F, 0.0F, 0.0F),   // Z+
    CreateRotated(180.0F, 0.0F, 0.0F), // Z-
};

const static Math::Matrix4x4 ProjectionMatrix =
    Math::Matrix4x4::Perspective(Math::DegToRad(90.0F), 1.0F, 0.1F, 100.0F);

const static Engine::CameraMatrices CameraMatrices{
    .ViewMatrix = Math::Matrix4x4::Identity(),
    .InverseViewMatrix = Math::Matrix4x4::Identity(),
    .ProjectionMatrix = ProjectionMatrix,
    .InverseProjectionMatrix = ProjectionMatrix.InverseTranspose(),
    .ViewProjectionMatrix = ProjectionMatrix,
    .InverseViewProjectionMatrix = ProjectionMatrix.InverseTranspose(),
    .RotationProjectionMatrix = ProjectionMatrix,
    .InverseRotationProjectionMatrix = ProjectionMatrix.InverseTranspose(),
};

auto LightprobePrefilterManager::PrefilterLightProbe(
    const Graphics::GraphicsContext &context, LightProbe &lightProbe,
    Engine::Scene *scene, const Transform &transform) -> Error {

  if (lightProbe.EnvironmentMapIndex < 0) {
    lightProbe.EnvironmentMapIndex = GetFreeEnvMapIndex().value_or(-1);
  }

  for (int i = 0; i < Transforms.size(); i++) {
    auto cubemapTransfrom = Transforms.at(i);
    cubemapTransfrom.SetPosition(transform.GetPosition());
    cubemapTransfrom.UpdateLocalMatrix();
    cubemapTransfrom.UpdateWorldMatrix(nullptr);

    Engine::CameraMatrices drawMatrices = CameraMatrices;

    const auto &worldMatrix = cubemapTransfrom.GetWorldMatrix();

    drawMatrices.ViewMatrix = worldMatrix.InverseTranspose();
    drawMatrices.InverseViewMatrix = worldMatrix;
    drawMatrices.RotationMatrix = drawMatrices.ViewMatrix.AsMatrix3x3();
    drawMatrices.InverseRotationMatrix =
        drawMatrices.RotationMatrix.Transpose();
    drawMatrices.Update();

    CHECK_ERR(Camera.WriteToBuffer(drawMatrices, cubemapTransfrom));

    Engine::DrawData drawData{
        .Transform = cubemapTransfrom,
        .Matrices = drawMatrices,
    };

    CHECK_ERR(Camera.Render(context, drawData, scene));

    Graphics::RenderState::SetShader({});

    CHECK_ERR(Graphics::RenderState::SetRenderTargets(
        context, {
                     Graphics::RenderState::RenderTarget{
                         .blendMode = Graphics::BlendmodeNone,
                         .texture = EnvMapArrayViews.at(i),
                         .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                     },
                 }));

    CHECK_ERR(Graphics::Draw(context,
                             *Camera.GetOwnedTextures().IncomingLight.get()));
  }

  CHECK_ERR(PrefilterEnvironmentMap(context, SceneCubemap, lightProbe));

  return {};
}

auto LightprobePrefilterManager::PrefilterEnvironment(
    const Graphics::GraphicsContext &context,
    const Ref<Graphics::Texture> &skyboxTexture) -> Error {
  auto lightProbe = LightProbe{};
  lightProbe.EnvironmentMapIndex = 0; // Skybox is always 0

  for (int i = 0; i < Transforms.size(); i++) {
    auto cubemapTransfrom = Transforms.at(i);
    cubemapTransfrom.UpdateLocalMatrix();
    cubemapTransfrom.UpdateWorldMatrix(nullptr);

    Engine::CameraMatrices drawMatrices = CameraMatrices;

    const auto &worldMatrix = cubemapTransfrom.GetWorldMatrix();

    drawMatrices.ViewMatrix = worldMatrix.InverseTranspose();
    drawMatrices.InverseViewMatrix = worldMatrix;
    drawMatrices.RotationMatrix = drawMatrices.ViewMatrix.AsMatrix3x3();
    drawMatrices.InverseRotationMatrix =
        drawMatrices.RotationMatrix.Transpose();
    drawMatrices.Update();

    CHECK_ERR(Camera.WriteToBuffer(drawMatrices, cubemapTransfrom));

    CHECK_ERR(Camera.RenderSkyboxOnly(
        context, Environment{.SkyboxTexture = skyboxTexture}));

    Graphics::RenderState::SetShader({});

    CHECK_ERR(Graphics::RenderState::SetRenderTargets(
        context, {
                     Graphics::RenderState::RenderTarget{
                         .blendMode = Graphics::BlendmodeNone,
                         .texture = EnvMapArrayViews.at(i),
                         .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                     },
                 }));

    CHECK_ERR(Graphics::Draw(context,
                             *Camera.GetOwnedTextures().DirectLighting.get()));
  }

  CHECK_ERR(PrefilterEnvironmentMap(context, SceneCubemap, lightProbe));

  return {};
}

auto LightprobePrefilterManager::Deinitialize() -> void {
  RadianceMaps.reset();
  RadianceMapViews.clear();

  IrradianceMaps.reset();
  IrradianceMapViews.clear();

  SceneCubemap.reset();
  EnvMapArrayViews.clear();
  Camera = Engine::Camera();

  TemporaryRadianceMap.reset();

  PrefilteredRadianceCoeffs.clear();
  PrefilteredRadianceCoeffsBuffer.reset();

  EnvMapUsed.fill(false);
}

} // namespace Engine::Renderer