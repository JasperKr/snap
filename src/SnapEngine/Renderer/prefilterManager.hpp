#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Renderer/lightProbe.hpp"
#include "Scene/camera.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {
struct LightprobePrefilterManager {
  // cubemap size
  constexpr static uint32_t PrefilteredRadianceCubeMapSize = 128;

  // Octahedral size
  constexpr static uint32_t PrefilteredRadianceMapsSize = 512;
  constexpr static uint32_t PrefilteredIrradianceMapsSize = 32;

  constexpr static uint32_t MaxEnvMaps = 512;
  constexpr static uint32_t EnvMapReallocationStep = 64;

  constexpr static uint32_t IblRoughnessOneLevel = 6;
  constexpr static uint32_t ProbeReflectionMipLevels = 7;

  auto Initialize(const Graphics::GraphicsContext &context) -> Error;

  auto Deinitialize() -> void;

  auto PrefilterLightProbe(const Graphics::GraphicsContext &context,
                           LightProbe &lightProbe, Engine::Scene *scene,
                           const Transform &transform) -> Error;

  auto PrefilterEnvironment(const Graphics::GraphicsContext &context,
                            const Ref<Graphics::Texture> &skyboxTexture)
      -> Error;

  auto GetIrradianceMaps() -> Ref<Graphics::Texture> { return IrradianceMaps; }
  auto GetRadianceMaps() -> Ref<Graphics::Texture> { return RadianceMaps; }

private:
  struct LevelParameters {
    uint32_t Level;
    float Glossiness;
    float Roughness;
    uint32_t Resolution;
  };

  std::array<LevelParameters, ProbeReflectionMipLevels> levelParameters{};
  Ref<Graphics::Texture> SceneCubemap;

  Ref<Graphics::Texture> RadianceMaps;
  std::vector<Ref<Graphics::Texture>> RadianceMapViews;

  Ref<Graphics::Texture> IrradianceMaps;
  std::vector<Ref<Graphics::Texture>> IrradianceMapViews;

  Ref<Graphics::Texture> TemporaryRadianceMap;

  std::vector<uint8_t> PrefilteredRadianceCoeffs;
  Ref<Graphics::Buffer> PrefilteredRadianceCoeffsBuffer;

  std::array<bool, MaxEnvMaps> EnvMapUsed{};

  Engine::Camera Camera;

  std::vector<Ref<Graphics::Texture>> EnvMapArrayViews;

  static auto GlossinessToRoughness(float glossiness) -> float {
    constexpr float GGX_MAX_SPECULAR_POWER = 18.0F;

    // NOLINTBEGIN
    float exponent = std::pow(2.0F, glossiness * GGX_MAX_SPECULAR_POWER);

    return std::pow(2.0F / (1.0F + exponent), 0.25F);
    // NOLINTEND
  }

  auto CreateStorageTextures(const Graphics::GraphicsContext &context,
                             uint32_t arrayLayers) -> Error;

  auto GetFreeEnvMapIndex() -> std::optional<int32_t> {
    // Zero is reserved for the default environment map, so we start searching from 1

    for (int32_t i = 1; i < MaxEnvMaps; i++) {
      if (!EnvMapUsed.at(i)) {
        EnvMapUsed.at(i) = true;
        return i;
      }
    }

    return {};
  }

  auto FreeEnvMapIndex(int32_t index) -> void {
    // Zero is reserved for the default environment map, so we don't free it

    if (index >= MaxEnvMaps || index <= 0) {
      return;
    }

    EnvMapUsed.at(index) = false;
  }

  auto PrefilterRadianceMap(const Graphics::GraphicsContext &context,
                            const Ref<Graphics::Texture> &envMap,
                            const Ref<Graphics::Texture> &output,
                            const LightProbe &lightProbe) -> Error;

  auto PrefilterIrradianceMap(const Graphics::GraphicsContext &context,
                              int32_t slice) -> Error;

  auto PrefilterEnvironmentMap(const Graphics::GraphicsContext &context,
                               const Ref<Graphics::Texture> &envMap,
                               LightProbe &lightProbe) -> Error;
};
} // namespace Engine::Renderer