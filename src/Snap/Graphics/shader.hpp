#pragma once

#include "Graphics/Buffers/push.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/localState.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "graphics.hpp"
#include "reflect.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <memory>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics {
struct StructuredBuffer;
}

namespace Graphics {

static const Type LuaShaderType = Type("Shader");

constexpr auto
ShaderStageFlagsToPipelineStageFlags(VkShaderStageFlags shaderStages)
    -> VkPipelineStageFlags2 {
  VkPipelineStageFlags2 pipelineStages = 0;

  if (shaderStages == VK_SHADER_STAGE_ALL) {
    return VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  }

  if ((shaderStages & VK_SHADER_STAGE_VERTEX_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_GEOMETRY_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_FRAGMENT_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  }
  if ((shaderStages & VK_SHADER_STAGE_COMPUTE_BIT) != 0U) {
    pipelineStages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }

  return pipelineStages;
}

using BoundBufferPair = std::pair<Ref<Buffer>, const Reflect::BufferInfo *>;
using BoundTexturePair = std::pair<Ref<Texture>, const Reflect::SamplerInfo *>;
using BoundASPair =
    std::pair<Ref<TLAS>, const Reflect::AccelerationStructureInfo *>;

struct ResourceBinding {
  uint32_t binding{};
  ObjectID resource{};

  bool isDynamic = false;

  auto operator==(const ResourceBinding &other) const -> bool {
    return binding == other.binding && resource == other.resource &&
           isDynamic == other.isDynamic;
  }
};

struct BoundState {
  std::unordered_map<uint64_t, BoundBufferPair> userBoundBuffers;
  std::unordered_map<uint64_t, BoundTexturePair> userBoundTextures;
  std::unordered_map<uint64_t, BoundASPair> userBoundAccelerationStructures;
  std::unordered_map<uint32_t, ResourceBinding> bindingInfos;
};

struct Shader : Object, Identifiable {
  Shader() = default;
  Shader(const Shader &) = delete;
  Shader(Shader &&) = delete;
  auto operator=(const Shader &) -> Shader & = delete;
  auto operator=(Shader &&) -> Shader & = delete;
  std::string moduleName;

  VkShaderModule module{};
  std::vector<std::pair<std::string, VkShaderStageFlagBits>> entryPoints;
  VkShaderStageFlagBits combinedShaderStages{};
  VkPipelineStageFlagBits2 combinedPipelineStages{};

  uint64_t modTime{};

  std::string name;

  slang::ProgramLayout *programLayout = nullptr;
  Slang::ComPtr<slang::IModule> slangModule = nullptr;
  Slang::ComPtr<slang::IComponentType> linkedProgram;

  std::unordered_map<SlangStage, size_t> entryPointToStageIndex;

  // Set -> (Texture, Sampler)
  std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>
      bindingInfos;

  Reflect::ShaderReflection reflection;
  std::unique_ptr<PushBuffer> pushBuffer;

  std::vector<slang::PreprocessorMacroDesc> preprocessorMacros;

  static ThreadLocalState<BoundState> BoundStateManager;
  auto GetState() const -> BoundState & {
    return BoundStateManager.GetState(*this);
  }

  std::vector<uint8_t> globalUniforms;

  Math::Uvec3 threadgroupSize{1, 1, 1};
  uint32_t waveSize = 0;

  ~Shader() override {
    auto *ctx = GetCurrentGraphicsContext();

    auto &state = GetState();
    state.userBoundBuffers.clear();
    state.userBoundTextures.clear();

    BoundStateManager.EraseState(*this);

    ScheduleDestruction(
        ShaderModuleMemory{
            .shaderModule = module,
        },
        Graphics::SemaphoreManager::GetSemaphoreValue());
  }

  static auto Create(const GraphicsContext &context,
                     const std::string &modulename, const std::string &name,
                     const std::vector<slang::PreprocessorMacroDesc>
                         *preprocessorMacros = nullptr) -> Result<Ref<Shader>>;

  auto Send(const ResourceKey &key, const std::span<const uint8_t> &data)
      -> Error;

  auto Send(const ResourceKey &key, const Ref<Buffer> &buffer) -> Error;

  auto Send(const ResourceKey &key, const Ref<Graphics::Texture> &texture)
      -> Error;
  auto Send(const ResourceKey &key,
            const Ref<::Graphics::StructuredBuffer> &buffer) -> Error;

  auto Send(const ResourceKey &key, const Ref<TLAS> &accelStructure) -> Error;

  auto GetUniform(const ResourceKey &key) const
      -> const Reflect::ResourceInfo *;

  auto GetSlotDescription(uint32_t set, uint32_t binding)
      -> Reflect::ResourceInfo *;
  auto GetSlotDescription(uint64_t slot) -> Reflect::ResourceInfo *;

  auto GetThreadgroupSize() const -> Result<Math::Uvec3>;
  auto GetWaveSize() const -> uint32_t;

  auto operator==(const Shader &other) const -> bool {
    if (module != other.module) {
      return false;
    }

    if (entryPoints != other.entryPoints) {
      return false;
    }

    return true;
  }

  auto hash() const -> size_t;

  static auto GetType() -> Type const * { return &LuaShaderType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return Shader::GetType();
  }
};

extern Ref<Shader> DefaultShaderModule;             // NOLINT
extern std::vector<const char *> ShaderSearchPaths; // NOLINT

auto SlangStageToVkStage(SlangStage stage) -> VkShaderStageFlagBits;

auto LoadShaderModule() -> Error;
void UnloadShaderModule(const GraphicsContext &context);

} // namespace Graphics
