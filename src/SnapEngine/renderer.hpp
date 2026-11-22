#pragma once

#include "Editor/lineDrawer.hpp"
#include "Editor/primitiveDrawer.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/allocator.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Renderer/bloomManager.hpp"
#include "Renderer/prefilterManager.hpp"
#include "Renderer/shaderManager.hpp"
#include "Renderer/volumeManager.hpp"
#include "bufferUploadManager.hpp"
#include "material.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
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
        .name = "ReflectanceFactor",
        .format = VK_FORMAT_R32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "Padding0",
        .format = VK_FORMAT_R32_UINT,
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
        .name = "CullMode",
        .format = VK_FORMAT_R32_UINT,
    },
    Graphics::BufferComponent{
        .name = "TextureUVIndices",
        .format = VK_FORMAT_R32G32_UINT,
    },

    Graphics::BufferComponent{
        .name = "AlbedoAddress",
        .format = VK_FORMAT_R32G32_UINT,
    },
    Graphics::BufferComponent{
        .name = "NormalAddress",
        .format = VK_FORMAT_R32G32_UINT,
    },
    Graphics::BufferComponent{
        .name = "MetallicRoughnessAddress",
        .format = VK_FORMAT_R32G32_UINT,
    },
    Graphics::BufferComponent{
        .name = "AmbientOcclusionAddress",
        .format = VK_FORMAT_R32G32_UINT,
    },
    Graphics::BufferComponent{
        .name = "ReflectanceAddress",
        .format = VK_FORMAT_R32G32_UINT,
    },
    Graphics::BufferComponent{
        .name = "EmissiveAddress",
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

const std::vector<Graphics::BufferComponent> LODComponents = {
    Graphics::BufferComponent{
        .name = "Vertices",
        .format = VK_FORMAT_R32G32_UINT,
    },
    Graphics::BufferComponent{
        .name = "DrawRanges",
        .format = VK_FORMAT_R32G32_UINT,
        .arraySize = 8,
    },
    Graphics::BufferComponent{
        .name = "LodRanges",
        .format = VK_FORMAT_R32_SFLOAT,
        .arraySize = 8,
    },
    Graphics::BufferComponent{
        .name = "BoundsMin",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
    },
    Graphics::BufferComponent{
        .name = "BoundsMax",
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
    },
};

const std::vector<Graphics::BufferComponent> MeshBufferComponents = {
    Graphics::BufferComponent{
        .name = "Lod",
        .format = Graphics::BufferFormat(LODComponents),
    },
    Graphics::BufferComponent{
        .name = "MaterialIndex",
        .format = VK_FORMAT_R32_UINT,
    },
    Graphics::BufferComponent{
        .name = "TransformIndex",
        .format = VK_FORMAT_R32_UINT,
    },
};

struct GPULOD {
  Math::Uvec2 VerticesBuffer{};
  std::array<Math::Uvec2, 8> DrawRanges{};
  std::array<float, 8> LODRanges{};

  Math::Vec4 BoundsMin;
  Math::Vec4 BoundsMax;
};

struct GPUMesh {
  GPULOD Lod;
  uint32_t MaterialIndex{};
  uint32_t TransformIndex{};
};

const Graphics::BufferFormat LightProbeBufferFormat{LightProbeBufferComponents};

struct Renderer {
  static constexpr uint32_t MaxDrawsPerPass = 1024UL * 32U; // n opaque front facing, n opaque back facing, n masked none, etc

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

  /*
    opaque prepass: NONE, BACK, FRONT
    masked prepass: NONE, BACK, FRONT
    transparency  : NONE, BACK, FRONT
  */
  struct DrawBuffers {
    std::array<Ref<Graphics::Buffer>, 9> DrawLists;    // 9x draw lists
    Ref<Graphics::Buffer> DrawIndices;                 // 9 element uint buffer
    std::array<Ref<Graphics::Buffer>, 9> DrawCommands; // 9x draw commands
    std::array<Ref<Graphics::Buffer>, 9> MeshIndices;  // Vertex range -> mesh index
  };

  auto Initialize(Graphics::GraphicsContext &context) -> Error;

  void Deinitialize();

  auto GetNewMaterialIndex() -> Result<size_t>;

  auto BindLightBuffers(const Graphics::GraphicsContext &context, const Ref<Graphics::Shader> &shader) const -> Error;

  auto GetDefaultMaterial() const -> const Material & { return DefaultMaterial; }
  auto GetNoMaterial() const -> const Material & { return NoMaterial; }
  auto GetMaterialsBuffer() const -> Ref<Graphics::StructuredBuffer> { return MaterialsBuffer; }
  auto GetModelTransformsBuffer() const -> Ref<Graphics::StructuredBuffer> { return ModelTransformsBuffer; }
  auto AssureModelTransformBufferSize(size_t minimumSize) -> Error;

  auto GetSceneLightBuffers() -> Lights & { return SceneLightBuffers; }
  auto GetSceneLightBuffers() const -> const Lights & { return SceneLightBuffers; }

  auto GetPrefilterManager() -> LightprobePrefilterManager & { return PrefilterManager; }
  auto GetPrefilterManager() const -> const LightprobePrefilterManager & { return PrefilterManager; }

  auto NewFrame(const Graphics::GraphicsContext &context) -> Error;

  auto GetLightProbeBuffer() const -> Ref<Graphics::StructuredBuffer> { return LightProbeBuffer; }

  auto GetLineDrawer() -> LineDrawer & { return LineDrawer; }
  auto GetLineDrawer() const -> const LineDrawer & { return LineDrawer; }
  auto GetPrimitiveDrawer() -> PrimitiveDrawer & { return PrimitiveDrawer; }
  auto GetPrimitiveDrawer() const -> const PrimitiveDrawer & { return PrimitiveDrawer; }
  auto GetBloomManager() -> BloomManager & { return BloomManager; }
  auto GetBloomManager() const -> const BloomManager & { return BloomManager; }
  auto GetShaderManager() -> ShaderManager & { return ShaderManager; }
  auto GetShaderManager() const -> const ShaderManager & { return ShaderManager; }
  auto GetSceneTLAS() -> Ref<Graphics::TLAS> & { return SceneTLAS; }
  auto GetSceneTLAS() const -> const Ref<Graphics::TLAS> & { return SceneTLAS; }
  auto SceneNeedsTLASRebuild() const -> bool { return SceneTLASNeedsRebuild; }
  auto SetSceneNeedsTLASRebuild(bool needsRebuild) -> void { SceneTLASNeedsRebuild = needsRebuild; }
  auto GetBlueNoiseTexture() -> Ref<Graphics::Texture> & { return BlueNoiseTexture; }
  auto GetBlueNoiseTexture() const -> const Ref<Graphics::Texture> & { return BlueNoiseTexture; }
  auto GetSamplesBuffer() -> Ref<Graphics::Buffer> & { return PMJ02bnSamples; }
  auto GetSamplesBuffer() const -> const Ref<Graphics::Buffer> & { return PMJ02bnSamples; }

  auto GetMaterialUploadManager() -> BufferUploadManager & { return MaterialUploadManager; }
  auto GetVolumeManager() -> VolumeManager & { return VolumeManager; }
  auto GetBLASPool() -> Graphics::BLASPool * { return &BLASPool; }

  auto GetIsInitialized() const -> bool { return initialized; }

  /*
  opaque prepass: NONE, BACK, FRONT

  masked prepass: NONE, BACK, FRONT

  transparency  : NONE, BACK, FRONT
  */
  DrawBuffers DrawBuffers;

  bool initialized = false;

  Material NoMaterial;
  Material DefaultMaterial;

  Graphics::GapAllocator MaterialAllocator{112, sizeof(Material::MaterialData)};
  Ref<Graphics::StructuredBuffer> MaterialsBuffer;
  BufferUploadManager MaterialUploadManager;

  Graphics::GapAllocator TransformAllocator{128, sizeof(Math::Matrix4x4) * 2};
  Ref<Graphics::StructuredBuffer> ModelTransformsBuffer;

  Graphics::GapAllocator MeshAllocator{160, 160}; // 160 is the stride of GPUMesh with std430
  Ref<Graphics::StructuredBuffer> MeshBuffer;

  Ref<Graphics::StructuredBuffer> LightProbeBuffer;

  Lights SceneLightBuffers;
  LightprobePrefilterManager PrefilterManager;

  LineDrawer LineDrawer;
  PrimitiveDrawer PrimitiveDrawer;
  BloomManager BloomManager;

  ShaderManager ShaderManager;

  Ref<Graphics::TLAS> SceneTLAS;
  bool SceneTLASNeedsRebuild = true;

  Ref<Graphics::Texture> BlueNoiseTexture;
  Ref<Graphics::Buffer> PMJ02bnSamples;

  VolumeManager VolumeManager;
  Graphics::BLASPool BLASPool;

  auto InitializeMaterialBuffer(const Graphics::GraphicsContext &context, size_t initialSize) -> Error;
  auto InitializeLightBuffers(const Graphics::GraphicsContext &context) -> Error;
  auto InitializeModelTransformsBuffer(const Graphics::GraphicsContext &context, size_t initialSize) -> Error;
  auto InitializeMeshBuffer(const Graphics::GraphicsContext &context, size_t initialSize) -> Error;

  auto InitializeDefaultMaterial(Graphics::GraphicsContext &context) -> Error;
};

auto DrawFullScreen(const Graphics::GraphicsContext &context) -> Error;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Renderer RendererInstance;

} // namespace Engine::Renderer