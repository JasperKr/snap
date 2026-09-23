#pragma once

#include "Editor/lineDrawer.hpp"
#include "Editor/primitiveDrawer.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Renderer/bloomManager.hpp"
#include "Renderer/prefilterManager.hpp"
#include "Renderer/shaderManager.hpp"
#include "bufferUploadManager.hpp"
#include "material.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

const std::vector<Graphics::BufferComponent> MaterialBufferComponents = {
    Graphics::BufferComponent{
        .name = "AlbedoFactor",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "RoughnessFactor",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "MetallicFactor",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "EmissiveFactor",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "AlphaMode",
        .format = VK_FORMAT_R32_UINT,
    },
    Graphics::BufferComponent{
        .name = "AlphaCutoff",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "TextureUVIndices",
        .format = VK_FORMAT_R32G32_UINT,
    },
};

const std::vector<Graphics::BufferComponent> ModelTransformBufferComponents = {
    Graphics::BufferComponent{
        .name = "ModelMatrix",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .arraySize = 4,
        .isMatrix = true,
    },
    Graphics::BufferComponent{
        .name = "NormalMatrix",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .arraySize = 3,
        .isMatrix = true,
    },
};

const std::vector<Graphics::BufferComponent> LightProbeBufferComponents = {
    Graphics::BufferComponent{
        .name = "Position",
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Index",
        .format = VK_FORMAT_R32_UINT,
    },
    Graphics::BufferComponent{
        .name = "Radius",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "InnerRadius",
        .format = VK_FORMAT_R32_SFLOAT,
    },
};

const Graphics::BufferFormat LightProbeBufferFormat{LightProbeBufferComponents};

struct Renderer {
  struct Lights {
    Ref<Graphics::StructuredBuffer> PointLightsBuffer;
    Ref<Graphics::StructuredBuffer> SpotLightsBuffer;
    Ref<Graphics::StructuredBuffer> RectangleLightsBuffer;
    Ref<Graphics::StructuredBuffer> SphereLightsBuffer;
    Ref<Graphics::StructuredBuffer> DirectionalLightsBuffer;

    std::vector<uint8_t> PointLightData;
    std::vector<uint8_t> SpotLightData;
    std::vector<uint8_t> RectangleLightData;
    std::vector<uint8_t> SphereLightData;
    std::vector<uint8_t> DirectionalLightData;

    int PointLightCount = 0;
    int SpotLightCount = 0;
    int RectangleLightCount = 0;
    int SphereLightCount = 0;
    int DirectionalLightCount = 0;
  };

  auto Initialize(Graphics::GraphicsContext &context) -> Error {
    constexpr size_t InitialMaterialBufferSize = 1024UL;
    constexpr size_t InitialModelTransformBufferSize = 4096UL;

    CHECK_ERR(InitializeDefaultMaterial(context));

    CHECK_ERR(InitializeMaterialBuffer(context, InitialMaterialBufferSize));
    CHECK_ERR(InitializeModelTransformsBuffer(context,
                                              InitialModelTransformBufferSize));

    CHECK_ERR(InitializeLightBuffers(context));
    CHECK_ERR(PrefilterManager.Initialize(context));

    LightProbeBuffer = CHECK_RES(Graphics::StructuredBuffer::Create(
        context, LightProbeBufferFormat, 64UL,
        Graphics::StructuredBufferCreationInfo{
            .memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .usageFlags =
                static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
                static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_DST_BIT) |
                static_cast<uint32_t>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
            .debugName = "Light Probe Buffer",
        }));

    CHECK_ERR(LineDrawer.Initialize(context));
    CHECK_ERR(PrimitiveDrawer.Initialize(context));
    CHECK_ERR(BloomManager.Initialize(context));
    SceneTLAS = CHECK_RES(Graphics::TLAS::Create(context));
    BlueNoiseTexture = CHECK_RES(Graphics::Texture::FromFile(
        context, "src/Scripting/Graphics/Assets/blue_noise_128.bmp",
        VK_IMAGE_USAGE_SAMPLED_BIT, Graphics::TextureMipmapOption::None));
    BlueNoiseTexture->SetWrapmode(VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  VK_SAMPLER_ADDRESS_MODE_REPEAT);

    auto samples = CHECK_RES(
        Filesystem::ReadFile("src/Snap/Graphics/Assets/pmj_samples.bin"));
    Graphics::BufferCreationInfo info = {
        .size = samples.size(),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .stagingBuffer = false,
        .persistentMapping = false,
        .debugName = "Progressive Multi-Jittered (0,2) Blue Noise Samples",
    };

    PMJ02bnSamples = CHECK_RES(Graphics::Buffer::Create(context, info));
    CHECK_ERR(PMJ02bnSamples->SetData(context, samples))

    CHECK_ERR(ShaderManager.Preload());

    initialized = true;
    return {};
  }

  void Deinitialize();

  auto GetNewMaterialIndex() -> Result<size_t>;

  auto BindLightBuffers(const Graphics::GraphicsContext &context,
                        const Ref<Graphics::Shader> &shader) const -> Error;

  auto GetDefaultMaterial() const -> const Material & {
    return DefaultMaterial;
  }
  auto GetNoMaterial() const -> const Material & { return NoMaterial; }
  auto GetMaterialsBuffer() const -> Ref<Graphics::StructuredBuffer> {
    return MaterialsBuffer;
  }
  auto GetModelTransformsBuffer() const -> Ref<Graphics::StructuredBuffer> {
    return ModelTransformsBuffer;
  }
  auto AssureModelTransformBufferSize(size_t minimumSize) -> Error;

  auto GetSceneLightBuffers() -> Lights & { return SceneLightBuffers; }
  auto GetSceneLightBuffers() const -> const Lights & {
    return SceneLightBuffers;
  }

  auto GetPrefilterManager() -> LightprobePrefilterManager & {
    return PrefilterManager;
  }
  auto GetPrefilterManager() const -> const LightprobePrefilterManager & {
    return PrefilterManager;
  }

  auto NewFrame() -> void { ModelTransformBufferElementCount = 0; }

  auto GetLightProbeBuffer() -> Ref<Graphics::StructuredBuffer> {
    return LightProbeBuffer;
  }

  auto GetLineDrawer() -> LineDrawer & { return LineDrawer; }
  auto GetLineDrawer() const -> const LineDrawer & { return LineDrawer; }
  auto GetPrimitiveDrawer() -> PrimitiveDrawer & { return PrimitiveDrawer; }
  auto GetPrimitiveDrawer() const -> const PrimitiveDrawer & {
    return PrimitiveDrawer;
  }
  auto GetBloomManager() -> BloomManager & { return BloomManager; }
  auto GetBloomManager() const -> const BloomManager & { return BloomManager; }
  auto GetShaderManager() -> ShaderManager & { return ShaderManager; }
  auto GetShaderManager() const -> const ShaderManager & {
    return ShaderManager;
  }
  auto GetSceneTLAS() -> Ref<Graphics::TLAS> & { return SceneTLAS; }
  auto GetSceneTLAS() const -> const Ref<Graphics::TLAS> & { return SceneTLAS; }
  auto SceneNeedsTLASRebuild() const -> bool { return SceneTLASNeedsRebuild; }
  auto SetSceneNeedsTLASRebuild(bool needsRebuild) -> void {
    SceneTLASNeedsRebuild = needsRebuild;
  }
  auto GetBlueNoiseTexture() -> Ref<Graphics::Texture> & {
    return BlueNoiseTexture;
  }
  auto GetBlueNoiseTexture() const -> const Ref<Graphics::Texture> & {
    return BlueNoiseTexture;
  }
  auto GetSamplesBuffer() -> Ref<Graphics::Buffer> & { return PMJ02bnSamples; }
  auto GetSamplesBuffer() const -> const Ref<Graphics::Buffer> & {
    return PMJ02bnSamples;
  }

  auto GetMaterialUploadManager() -> BufferUploadManager & {
    return MaterialUploadManager;
  }

private:
  Material NoMaterial;
  Material DefaultMaterial;

  Ref<Graphics::StructuredBuffer> MaterialsBuffer;
  std::unordered_set<size_t> UsedMaterialIndices;
  BufferUploadManager MaterialUploadManager;

  Ref<Graphics::StructuredBuffer> ModelTransformsBuffer;
  size_t ModelTransformBufferElementCount = 0;

  Ref<Graphics::StructuredBuffer> LightProbeBuffer;

  Lights SceneLightBuffers;
  LightprobePrefilterManager PrefilterManager;

  LineDrawer LineDrawer;
  PrimitiveDrawer PrimitiveDrawer;
  BloomManager BloomManager;

  ShaderManager ShaderManager;

  Ref<Graphics::TLAS> SceneTLAS;
  bool SceneTLASNeedsRebuild = true;

  bool initialized = false;
  Ref<Graphics::Texture> BlueNoiseTexture;
  Ref<Graphics::Buffer> PMJ02bnSamples;

  auto InitializeMaterialBuffer(Graphics::GraphicsContext &context,
                                size_t initialSize) -> Error;
  auto InitializeLightBuffers(Graphics::GraphicsContext &context) -> Error;
  auto InitializeModelTransformsBuffer(Graphics::GraphicsContext &context,
                                       size_t initialSize) -> Error;

  auto InitializeDefaultMaterial(Graphics::GraphicsContext &context) -> Error;
};

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer