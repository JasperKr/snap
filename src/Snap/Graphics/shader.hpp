#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/descriptorSlots.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/localState.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "reflect.hpp"
#include "slang/slang.h"
#include <cstdint>
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

constexpr auto ShaderStageFlagsToPipelineStageFlags(VkShaderStageFlags shaderStages) -> VkPipelineStageFlags2;

// NOLINTBEGIN(hicpp-explicit-conversions)
template <typename T> struct BoundShaderResource {
  VkAccessFlags2 access = VK_ACCESS_2_NONE;
  VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
  Ref<T> resource;
};

// Any bound AS == TLAS == AS-Read-only thus no extra info is needed.
using BoundASPair = Ref<TLAS>;

struct BoundState {
  std::unordered_map<uint32_t, BoundShaderResource<Buffer>> userBoundBuffers;
  std::unordered_map<uint32_t, BoundShaderResource<Texture>> userBoundTextures;
  std::unordered_map<uint32_t, BoundShaderResource<TLAS>> userBoundAccelerationStructures;
  std::vector<uint8_t> uniforms;
};

struct SampledTexture : BoundShaderResource<Texture> {
  SampledTexture(const Ref<Texture> &texture) {
    resource = texture;
    access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  }
};

struct StorageTexture : BoundShaderResource<Texture> {
  enum StorageRW : uint64_t {
    read = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
    write = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    readwrite = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
  };

  StorageTexture(const Ref<Texture> &texture, uint64_t access) {
    resource = texture;
    this->access = access;
    type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  }
};

struct UniformBuffer : BoundShaderResource<Buffer> {
  UniformBuffer(const Ref<Buffer> &buffer) {
    resource = buffer;
    access = VK_ACCESS_2_UNIFORM_READ_BIT;
    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }

  UniformBuffer(const Ref<StructuredBuffer> &buffer);
};

struct DynamicUniformBuffer : BoundShaderResource<Buffer> {
  uint32_t offset = 0;
  uint32_t size = 0;

  DynamicUniformBuffer(const Ref<Buffer> &buffer, uint32_t offset, uint32_t size) {
    resource = buffer;
    access = VK_ACCESS_2_UNIFORM_READ_BIT;
    type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }

  DynamicUniformBuffer(const Ref<StructuredBuffer> &buffer, uint32_t offset, uint32_t size);
};

struct StorageBuffer : BoundShaderResource<Buffer> {
  enum StorageRW : uint64_t {
    read = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
    write = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    readwrite = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
  };

  StorageBuffer(const Ref<Buffer> &buffer, uint64_t access) {
    resource = buffer;
    this->access = access;
    type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  }

  StorageBuffer(const Ref<StructuredBuffer> &buffer, uint64_t access);
};

//NOLINTEND(hicpp-explicit-conversions)

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

  Reflect::ShaderReflection reflection;

  std::vector<slang::PreprocessorMacroDesc> preprocessorMacros;

  static ThreadLocalState<BoundState> BoundStateManager;
  [[nodiscard]] auto GetState() const -> BoundState &;

  VkDeviceSize uniformsSize{};
  Math::Uvec3 threadgroupSize{1, 1, 1};
  uint32_t waveSize = 0;

  ~Shader() override;

  static auto Create(const GraphicsContext &context, const std::string &modulename, const std::string &name,
                     const std::vector<slang::PreprocessorMacroDesc> *preprocessorMacros = nullptr) -> Result<Ref<Shader>>;

  auto Send(const ResourceKey &key, const std::span<const uint8_t> &data) const -> Error;
  auto Send(ResolvedKey key, const std::span<const uint8_t> &data) const -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key, const UniformBuffer &buffer) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key, const DynamicUniformBuffer &buffer) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key, const StorageBuffer &buffer) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key, const SampledTexture &texture) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key, const StorageTexture &texture) -> Error;

  auto Send(const GraphicsContext &context, const ResourceKey &key, const Ref<TLAS> &accelStructure) -> Error;

  auto GetUniform(const ResourceKey &key) const -> const Reflect::ResourceInfo *;
  auto GetUniform(ResolvedKey key) const -> const Reflect::ResourceInfo &;

  auto ResolveKey(const ResourceKey &key) const -> Result<ResolvedKey>;

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

  [[nodiscard]] auto GetInstanceType() const -> Type const * override { return Shader::GetType(); }

private:
  struct CombinedImageSampler {
    uint32_t image;
    uint32_t sampler;
  };

  auto Send(ResolvedKey key, uint64_t data) const -> Error;
  auto Send(ResolvedKey key, CombinedImageSampler address) const -> Error;
};

extern Ref<Shader> DefaultShaderModule;             // NOLINT
extern std::vector<const char *> ShaderSearchPaths; // NOLINT

auto SlangStageToVkStage(SlangStage stage) -> VkShaderStageFlagBits;

auto LoadShaderModule() -> Error;
void UnloadShaderModule(const GraphicsContext &context);

} // namespace Graphics
