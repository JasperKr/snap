#pragma once

#include "Graphics/shader.hpp"
#include "Modules/error.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_map>
namespace Engine::Renderer {

enum class ShaderKey : uint8_t {
  DepthPrepass,
  DepthMaskPass,
  Deferred,
  SimpleLighting,
  PostProcessing,
  Skybox,
  FillSkybox,
  TransparencyForward,
  ApplyEnvironmentMap,
  DownsampleEnvironmentMap,
  EnvironmentMapToOctahedral,
  PrefilterRadiance,
  PrefilterIrradiance,
  StoreEnvironmentMap,
  ObjectPicker,
  VolumetricLighting,
  WriteVolumetrics,

  Shadows_Directional,
  Shadows_Point,
  Shadows_Spot,
  Shadows_Sphere,
  Shadows_Rectangle,
  Shadows_Denoise,
  Shadows_ClearHitFlags,

  Clear,

  RTAO,
  AO_Denoise,

  TAA,
};

struct ShaderConfiguration {
  std::string path;
  std::string name;
  std::vector<slang::PreprocessorMacroDesc> externs;
};

const extern std::unordered_map<ShaderKey, ShaderConfiguration> ShaderConfigurations;

struct ShaderManager {
  auto Preload() -> Error;
  auto GetShader(ShaderKey shaderKey) -> Result<Ref<Graphics::Shader>>;
  auto ReloadShaders() -> void;

private:
  std::unordered_map<ShaderKey, Ref<Graphics::Shader>> LoadedShaders;

  enum class ShaderCompilationStatus : uint8_t {
    Unloaded,
    Created,
    Loading,
  };

  std::unordered_map<ShaderKey, ShaderCompilationStatus> CompilationStatuses;
};

// NOLINTBEGIN

extern std::mutex StatusMutex;
extern std::condition_variable CreatedCV;

// NOLINTEND

} // namespace Engine::Renderer