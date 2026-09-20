#include "shader.hpp"
#include "Graphics/Buffers/push.hpp"
#include "Graphics/Buffers/structured.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/reflect.hpp"
#include "Graphics/snapshot.hpp"
#include "Graphics/texture.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "graphics.hpp"
#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include <array>
#include <condition_variable>
#include <cstring>
#include <format>
#include <memory>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string_view>
#include <utility>

#include "vulkan/vulkan_core.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Graphics {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::unordered_map<VkShaderModule, BoundState> BoundStates;

static const uint32_t MaxConcurrentSlangSessionCount = 6;

static std::vector<slang::IGlobalSession *> SessionStorage; // For destruction
static std::vector<slang::IGlobalSession *> UnusedGlobalSlangSessions;
static std::condition_variable SlangSessionCV;
static std::mutex SlangSessionMutex;

thread_local slang::IGlobalSession *GlobalSlangSession;
static uint32_t SlangSessionCount = 0;
std::vector<const char *> ShaderSearchPaths;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto GetGlobalSlangSession() -> Result<slang::IGlobalSession *> {
  if (GlobalSlangSession == nullptr) {
    std::unique_lock<std::mutex> lock(SlangSessionMutex);

    if (SlangSessionCount < MaxConcurrentSlangSessionCount) {
      ZoneScopedN("Create new global session");

      CHECK_NEW_ERR(slang::createGlobalSession(&GlobalSlangSession));

      SessionStorage.emplace_back(GlobalSlangSession);
      SlangSessionCount++;
    } else {
      ZoneScopedN("Reuse existing global session");

      SlangSessionCV.wait(
          lock, [&]() -> bool { return !UnusedGlobalSlangSessions.empty(); });

      GlobalSlangSession = UnusedGlobalSlangSessions.back();
      UnusedGlobalSlangSessions.pop_back();
    }
  }

  return GlobalSlangSession;
}

inline auto ReleaseGlobalSlangSession() -> void {
  std::lock_guard<std::mutex> lock(SlangSessionMutex);

  if (GlobalSlangSession != nullptr) {
    UnusedGlobalSlangSessions.emplace_back(GlobalSlangSession);
    GlobalSlangSession = nullptr;

    SlangSessionCV.notify_one();
  }
}

static const std::vector<slang::CompilerOptionEntry> CompilerOptions = {
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::Optimization,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL,
            }},
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::EmitSpirvMethod,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = SlangEmitSpirvMethod::SLANG_EMIT_SPIRV_DIRECTLY,
            }},
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::VulkanUseEntryPointName,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 = 1,
            }},
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::DebugInformation,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 =
                    SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_MAXIMAL,
            }},
    slang::CompilerOptionEntry{
        .name = slang::CompilerOptionName::FloatingPointMode,
        .value =
            slang::CompilerOptionValue{
                .kind = slang::CompilerOptionValueKind::Int,
                .intValue0 =
                    SlangFloatingPointMode::SLANG_FLOATING_POINT_MODE_FAST,
            }},
};

// NOLINTNEXTLINE
static slang::TargetDesc SpvTargetDesc = {
    .format = SLANG_SPIRV,
    .profile = SLANG_PROFILE_UNKNOWN,
    .compilerOptionEntries = CompilerOptions.data(),
    .compilerOptionEntryCount = static_cast<uint32_t>(CompilerOptions.size()),
};

Ref<Shader> DefaultShaderModule = {}; // NOLINT

auto LoadShaderModule() -> Error {
  ZoneScoped;

  auto *session = CHECK_RES(GetGlobalSlangSession());
  SpvTargetDesc.profile = session->findProfile("spirv_1_5+spvRayQueryKHR");
  ReleaseGlobalSlangSession();

  DefaultShaderModule = CHECK_RES(Shader::Create(
      *GetCurrentGraphicsContext(), "default2D", "Default shader"));

  return Error::Success();
}

void UnloadShaderModule(const Graphics::GraphicsContext &context) {
  DefaultShaderModule.reset();

  for (auto *session : SessionStorage) {
    session->release();
  }

  SessionStorage.clear();
  slang::shutdown();
}

auto SlangStageToVkStage(SlangStage stage) -> VkShaderStageFlagBits {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case SLANG_STAGE_HULL:
    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  case SLANG_STAGE_DOMAIN:
    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  case SLANG_STAGE_GEOMETRY:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case SLANG_STAGE_FRAGMENT:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case SLANG_STAGE_COMPUTE:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case SLANG_STAGE_MESH:
    return VK_SHADER_STAGE_MESH_BIT_EXT;
  case SLANG_STAGE_AMPLIFICATION:
    return VK_SHADER_STAGE_TASK_BIT_EXT;
  case SLANG_STAGE_RAY_GENERATION:
    return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  case SLANG_STAGE_ANY_HIT:
    return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
  case SLANG_STAGE_CLOSEST_HIT:
    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  case SLANG_STAGE_MISS:
    return VK_SHADER_STAGE_MISS_BIT_KHR;
  case SLANG_STAGE_CALLABLE:
    return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
  default:
    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }
}

const std::vector<SlangStage> SlangStages = {
    SLANG_STAGE_VERTEX,   SLANG_STAGE_HULL,          SLANG_STAGE_DOMAIN,
    SLANG_STAGE_GEOMETRY, SLANG_STAGE_FRAGMENT,      SLANG_STAGE_COMPUTE,
    SLANG_STAGE_MESH,     SLANG_STAGE_AMPLIFICATION, SLANG_STAGE_RAY_GENERATION,
    SLANG_STAGE_ANY_HIT,  SLANG_STAGE_CLOSEST_HIT,   SLANG_STAGE_MISS,
    SLANG_STAGE_CALLABLE,
};

inline auto SlangStageToString(SlangStage stage) -> std::string_view {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return "vertex";
  case SLANG_STAGE_HULL:
    return "tessellationControl";
  case SLANG_STAGE_DOMAIN:
    return "tessellationEvaluation";
  case SLANG_STAGE_GEOMETRY:
    return "geometry";
  case SLANG_STAGE_FRAGMENT:
    return "fragment";
  case SLANG_STAGE_COMPUTE:
    return "compute";
  case SLANG_STAGE_MESH:
    return "mesh";
  case SLANG_STAGE_AMPLIFICATION:
    return "task";
  case SLANG_STAGE_RAY_GENERATION:
    return "rayGeneration";
  case SLANG_STAGE_ANY_HIT:
    return "anyHit";
  case SLANG_STAGE_CLOSEST_HIT:
    return "closestHit";
  case SLANG_STAGE_MISS:
    return "miss";
  case SLANG_STAGE_CALLABLE:
    return "callable";
  default:
    return "unknown";
  }
}

// NOLINTNEXTLINE
static inline auto LoadSlang(const GraphicsContext &context,
                             Ref<Shader> &shader) -> Error {
  ZoneScoped;

  slang::SessionDesc sessionDesc = {};
  sessionDesc.allowGLSLSyntax = false;
  sessionDesc.defaultMatrixLayoutMode =
      SlangMatrixLayoutMode::SLANG_MATRIX_LAYOUT_ROW_MAJOR;

  auto sourceBaseDir = Filesystem::GetSourceBaseDirectory();
  auto sourceDir = Filesystem::GetSourceDirectory();
  auto saveDir = Filesystem::GetSaveDirectory();

  auto shaderDirectory = Path::Join({"src", "Snap", "Graphics", "Shaders"});
  const auto &sourceBack = Path::Join(sourceDir, std::string(".."));

  std::vector<const char *> searchPaths = {
      sourceDir.c_str(),     sourceBack.c_str(),      saveDir.c_str(),
      sourceBaseDir.c_str(), shaderDirectory.c_str(),
  };

  searchPaths.insert(searchPaths.end(), ShaderSearchPaths.begin(),
                     ShaderSearchPaths.end());

  std::string directories = "To be searched:\n";
  for (const auto &path : searchPaths) {
    directories += " - " + std::string(path) + "\n";
  }

  sessionDesc.searchPaths = searchPaths.data();
  sessionDesc.searchPathCount = static_cast<uint32_t>(searchPaths.size());
  sessionDesc.targets = &SpvTargetDesc;
  sessionDesc.targetCount = 1;
  sessionDesc.preprocessorMacros = shader->preprocessorMacros.data();
  sessionDesc.preprocessorMacroCount =
      static_cast<uint32_t>(shader->preprocessorMacros.size());

  auto *globalSession = CHECK_RES(GetGlobalSlangSession());

  Slang::ComPtr<slang::ISession> session;
  {
    ZoneScopedN("Create session");

    auto result = globalSession->createSession(sessionDesc, session.writeRef());

    CHECK_NEW_ERR(result);
  }

  Slang::ComPtr<slang::IBlob> diagnosticsBlob;
  PrintDebug("Compiling shader: " + shader->moduleName);
  {
    ZoneScopedN("Load Module");
    shader->slangModule = session->loadModule(shader->moduleName.c_str(),
                                              diagnosticsBlob.writeRef());
  }

  if (diagnosticsBlob != nullptr) {
    auto diagnostics = std::string_view(
        static_cast<const char *>(diagnosticsBlob->getBufferPointer()),
        diagnosticsBlob->getBufferSize());

    if (diagnosticsBlob.get() == nullptr ||
        (diagnosticsBlob->getBufferSize() == 0U)) {
      return Error::Create("Unknown error");
    }

    return Error::Createf("Diagnostics:\n{}", diagnostics);
  }

  ERR_ASSERT(shader->slangModule != nullptr);

  auto entryPointCount = shader->slangModule->getDefinedEntryPointCount();
  std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints;
  entryPoints.reserve(entryPointCount);
  shader->entryPoints.reserve(entryPointCount);
  std::vector<SlangStage> stages;
  stages.reserve(entryPointCount);

  PrintDebug("Shader entry points:");
  PrintDebug(" - Count: " + std::to_string(entryPointCount));

  auto allowedEntryPointCount = SlangStages.size();
  for (SlangInt32 i = 0; i < allowedEntryPointCount; i++) {
    auto stage = SlangStages.at(i);
    auto entryPointName = std::format("{}Main", SlangStageToString(stage));

    Slang::ComPtr<slang::IEntryPoint> entryPoint = nullptr;

    auto result = shader->slangModule->findEntryPointByName(
        entryPointName.c_str(), entryPoint.writeRef());

    if (entryPoint.readRef() == nullptr || Error::IsError(result)) {
      continue;
    }

    shader->entryPointToStageIndex[stage] = entryPoints.size();
    entryPoints.emplace_back(entryPoint);

    auto vkStage = SlangStageToVkStage(stage);
    shader->entryPoints.emplace_back(entryPointName, vkStage);
    shader->combinedShaderStages = static_cast<VkShaderStageFlagBits>(
        static_cast<uint32_t>(shader->combinedShaderStages) | (uint)vkStage);

    stages.emplace_back(stage);

    PrintDebug(" - {}; index: {}", entryPointName,
               std::to_string(entryPoints.size() - 1));
  }

  std::vector<slang::IComponentType *> componentTypes;
  componentTypes.emplace_back(shader->slangModule);

  for (auto &entryPoint : entryPoints) {
    componentTypes.emplace_back(entryPoint);
  }

  Slang::ComPtr<slang::IComponentType> composedProgram;

  PrintDebug("Composing program...");
  {
    ZoneScopedN("Create composite component type");

    auto result = session->createCompositeComponentType(
        componentTypes.data(), static_cast<SlangInt>(componentTypes.size()),
        composedProgram.writeRef(), diagnosticsBlob.writeRef());

    CHECK_ERR(
        Error::Create(result, diagnosticsBlob, composedProgram.readRef()));
  }

  PrintDebug("Linking program...");

  {
    ZoneScopedN("Link program");

    auto result = composedProgram->link(shader->linkedProgram.writeRef(),
                                        diagnosticsBlob.writeRef());

    CHECK_ERR(Error::Create(result, diagnosticsBlob,
                            shader->linkedProgram.readRef()));
  }

  PrintDebug("Getting program layout...");

  {
    ZoneScopedN("Get program layout");

    shader->programLayout =
        shader->linkedProgram->getLayout(0, diagnosticsBlob.writeRef());

    if (diagnosticsBlob != nullptr) {
      auto diagnostics = std::string_view(
          static_cast<const char *>(diagnosticsBlob->getBufferPointer()),
          diagnosticsBlob->getBufferSize());
      return Error::Createf("Diagnostics:\n{}", diagnostics);
    }
  }

  ERR_ASSERT(!shader->entryPoints.empty());

  if (shader->entryPoints.at(0).second == VK_SHADER_STAGE_COMPUTE_BIT) {
    ERR_ASSERT(entryPointCount == 1);

    auto *entrypointReflection = shader->programLayout->getEntryPointByIndex(0);

    std::array<SlangUInt, 3> out_workgroupSize = {1, 1, 1};
    SlangUInt out_waveSize = 0;

    entrypointReflection->getComputeThreadGroupSize(3,
                                                    out_workgroupSize.data());
    entrypointReflection->getComputeWaveSize(&out_waveSize);

    shader->threadgroupSize =
        Math::Uvec3{static_cast<uint32_t>(out_workgroupSize[0]),
                    static_cast<uint32_t>(out_workgroupSize[1]),
                    static_cast<uint32_t>(out_workgroupSize[2])};
    shader->waveSize = static_cast<uint32_t>(out_waveSize);

    auto invocationlimit =
        context.deviceProperties.limits.maxComputeWorkGroupInvocations;

    Math::Uvec3 sizelimit{
        context.deviceProperties.limits.maxComputeWorkGroupSize[0],
        context.deviceProperties.limits.maxComputeWorkGroupSize[1],
        context.deviceProperties.limits.maxComputeWorkGroupSize[2],
    };

    ERR_ASSERT(out_workgroupSize[0] * out_workgroupSize[1] *
                   out_workgroupSize[2] <=
               invocationlimit);

    ERR_ASSERT(shader->threadgroupSize.x <= sizelimit.x);
    ERR_ASSERT(shader->threadgroupSize.y <= sizelimit.y);
    ERR_ASSERT(shader->threadgroupSize.z <= sizelimit.z);
  }

  Slang::ComPtr<slang::IBlob> spirvCode;

  shader->entryPoints.resize(entryPointCount);

  auto vkStages = VK_PIPELINE_STAGE_2_NONE;

  for (const auto &stage : shader->entryPoints) {
    switch (stage.second) {
    case VK_SHADER_STAGE_VERTEX_BIT:
      vkStages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
      break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
      vkStages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      break;
    case VK_SHADER_STAGE_COMPUTE_BIT:
      vkStages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      break;
    default:
      break;
    }
  }

  shader->combinedPipelineStages = vkStages;

  {
    ZoneScopedN("Get target code");

    auto result = shader->linkedProgram->getTargetCode(
        0, // targetIndex
        spirvCode.writeRef(), diagnosticsBlob.writeRef());

    CHECK_ERR(Error::Create(result, diagnosticsBlob, spirvCode.readRef()));
  }

  ReleaseGlobalSlangSession();

  PrintDebug("Creating Vulkan shader module...");

  size_t codeSize = spirvCode->getBufferSize();
  std::vector<uint32_t> data(codeSize / 4);
  memcpy(data.data(), spirvCode->getBufferPointer(), codeSize);

  // NOLINTNEXTLINE
  std::span<uint8_t> spirvCodeSpan(reinterpret_cast<uint8_t *>(data.data()),
                                   codeSize);

  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = data.size() * sizeof(uint32_t);
  moduleCreateInfo.pCode = data.data();
  {
    ZoneScopedN("Create vulkan shadermodule");

    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateShaderModule(context.device, &moduleCreateInfo,
                                       GetAllocationCallbacks(),
                                       &shader->module));

    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_SHADER_MODULE;

    // NOLINTNEXTLINE
    nameInfo.objectHandle = reinterpret_cast<uint64_t>(shader->module);
    nameInfo.pObjectName = shader->moduleName.c_str();

    vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
  }

#if Enable_Snapshots
  Snapshot::CaptureEvent(
      Snapshot::ShaderModuleCreateEvent(shader->module, shader->moduleName));
#endif

  PrintDebug("Shader module created successfully.");

  return Error::Success();
}

auto Shader::Create(
    const Graphics::GraphicsContext &context, const std::string &modulename,
    const std::string &name,
    const std::vector<slang::PreprocessorMacroDesc> *preprocessorMacros)
    -> Result<Ref<Shader>> {
  ZoneScoped;

  ZoneName(modulename.c_str(), modulename.size());

  Ref<Shader> shader = Ref<Shader>::Make();
  shader->name = name;
  shader->moduleName = modulename;

  if (name == "") {
    shader->name = modulename;
  }

  if (preprocessorMacros != nullptr) {
    shader->preprocessorMacros = *preprocessorMacros;
  }

  switch (Window::GetWindowContext()->colorSpace) {
  case Window::ColorSpace::GammaCorrect:
    shader->preprocessorMacros.emplace_back("__COLORSPACE_GAMMA_CORRECT", "1");
    break;
  case Window::ColorSpace::Linear:
    shader->preprocessorMacros.emplace_back("__COLORSPACE_LINEAR", "1");
    break;
  case Window::ColorSpace::HDR:
    shader->preprocessorMacros.emplace_back("__COLORSPACE_HDR", "1");
    break;
  }

  // Compile Slang to SPIR-V and create shader module
  CHECK_ERR(LoadSlang(context, shader));

  CHECK_ERR(ReflectShader(context, shader->programLayout, shader->reflection));
  ERR_ASSERT(shader->reflection.pushBuffers.size() <= 1);

  if (!shader->reflection.pushBuffers.empty()) {
    shader->pushBuffer =
        std::make_unique<PushBuffer>(shader->reflection.pushBuffers.front());
  }

#if Enable_Snapshots
  Snapshot::CaptureEvent(
      Snapshot::ShaderModuleCreateEvent(shader->module, shader->moduleName));
#endif

  shader->globalUniforms.resize(
      shader->reflection.globalBufferFormat.GetStride());

  // shader->slangModule->release();
  // shader->linkedProgram->release();

  shader->programLayout = nullptr;
  shader->slangModule = nullptr;
  shader->linkedProgram = nullptr;

  return shader;
}

inline auto ValidateBuffers(const Shader *shader) -> Error {
  ZoneScoped;
  // Loop over shader->reflection.resources, and check if all buffers are
  // set up in shader->buffers,
  // this is done outside the shader as the user must manage these
  // resources themselves.

  for (const auto &resource : shader->reflection.resources) {
    if (!resource.IsBuffer()) {
      continue;
    }

    const auto &bufferInfo = std::get<Reflect::BufferInfo>(resource.info);

    // Not sent by the user
    if (bufferInfo.bufferType == Reflect::BufferType::PushConstant) {
      continue;
    }

    auto locationKey =
        Utils::SetBindingToSlot(bufferInfo.set, bufferInfo.binding);

    if (!shader->GetState().userBoundBuffers.contains(locationKey)) {
      return Error::Createf("Buffer '{}' not set up in shader.", resource.name);
    }
  }

  return Error::Success();
}

inline auto ValidateAccelerationStructures(const Shader *shader) -> Error {
  ZoneScoped;
  for (const auto &resource : shader->reflection.resources) {
    if (!resource.IsAccelerationStructure()) {
      continue;
    }

    const auto &accelStructInfo =
        std::get<Reflect::AccelerationStructureInfo>(resource.info);

    auto locationKey =
        Utils::SetBindingToSlot(accelStructInfo.set, accelStructInfo.binding);

    if (!shader->GetState().userBoundAccelerationStructures.contains(
            locationKey)) {
      return Error::Createf("Acceleration structure '{}' not set up in shader.",
                            resource.name);
    }
  }

  return Error::Success();
}

void append(ResourceKey &dest, const ResourceKey &src) {
  dest.insert(dest.end(), src.begin(), src.end());
}

auto Shader::GetUniform(const ResourceKey &key) const
    -> const Reflect::ResourceInfo * {
  if (pushBuffer != nullptr) {
    if (const auto *info = pushBuffer->GetUniform(key)) {
      return info;
    }
  }

  const auto &globalInfo = reflection.flattened.at(reflection.globals.set);
  auto iter = globalInfo.keyToInfo.find(key);
  if (iter != globalInfo.keyToInfo.end()) {
    return &iter->second;
  }
  return nullptr;
}

auto Shader::Send(const ResourceKey &key, const std::span<const uint8_t> &data)
    -> Error {
  ZoneScopedN("Shader::Send data span");

  if (pushBuffer && pushBuffer->ContainsUniform(key)) {
    return pushBuffer->SetData(key, data);
  }

  const auto *info = GetUniform(key);

  [[unlikely]]
  if (info == nullptr) {
    auto keyname = Reflect::ResourceKeyToString(key);
    return Error::Createf("Uniform {} not found in shader reflection: {}",
                          keyname, name);
  }

  ERR_ASSERT(info->offset + data.size() <= globalUniforms.size());

  // NOLINTNEXTLINE, pointer arithmetic
  memcpy(globalUniforms.data() + info->offset, data.data(), data.size());

  return Error::Success();
}

auto Shader::Send(const ResourceKey &key, const Ref<Buffer> &buffer) -> Error {
  ZoneScopedN("Shader::Send structured buffer");

  if (!buffer.isValid()) {
    return Error::Create("Buffer is null.");
  }

  auto iter = reflection.keyToSlot.find(key);
  if (iter != reflection.keyToSlot.end()) {
    auto locationKey = iter->second;
    auto [set, binding] = Utils::SlotToSetBinding(locationKey);

    auto *info = GetSlotDescription(set, binding);
    auto *bufferInfo = std::get_if<Reflect::BufferInfo>(&info->info);
    if (bufferInfo == nullptr) {
      auto keyname = Reflect::ResourceKeyToString(key);
      return Error::Createf(
          "Resource {} is not a buffer in shader reflection: {}", keyname,
          name);
    }

    GetState().userBoundBuffers[locationKey] =
        BoundBufferPair{buffer, bufferInfo};
    return Error::Success();
  }

  auto keyname = Reflect::ResourceKeyToString(key);
  return Error::Createf("Buffer {} not found in shader reflection: {}", keyname,
                        name);
}

auto Shader::Send(const ResourceKey &key,
                  const Ref<::Graphics::StructuredBuffer> &buffer) -> Error {
  ZoneScopedN("Shader::Send structured buffer");
  return Send(key, buffer->GetBuffer());
}

auto Shader::Send(const ResourceKey &key, const Ref<TLAS> &accelStructure)
    -> Error {
  ZoneScopedN("Shader::Send acceleration structure");

  if (!accelStructure.isValid()) {
    return Error::Create("Acceleration structure is null.");
  }

  auto iter = reflection.keyToSlot.find(key);
  if (iter != reflection.keyToSlot.end()) {
    auto locationKey = iter->second;
    auto [set, binding] = Utils::SlotToSetBinding(locationKey);

    auto *info = GetSlotDescription(set, binding);

    if (info == nullptr) {
      auto keyname = Reflect::ResourceKeyToString(key);
      return Error::Createf("Resource {} is not found in shader reflection: {}",
                            keyname, name);
    }

    auto *accelStructInfo =
        std::get_if<Reflect::AccelerationStructureInfo>(&info->info);

    if (accelStructInfo == nullptr) {
      auto keyname = Reflect::ResourceKeyToString(key);
      return Error::Createf(
          "Resource {} is not an acceleration structure in shader reflection: "
          "{} ({})",
          keyname, name, info->ToString());
    }

    GetState().userBoundAccelerationStructures[locationKey] =
        BoundASPair{accelStructure, accelStructInfo};
    return Error::Success();
  }

  auto keyname = Reflect::ResourceKeyToString(key);
  return Error::Createf(
      "Acceleration structure {} not found in shader reflection: {}", keyname,
      name);
}

auto Shader::Send(const ResourceKey &key, const Ref<Graphics::Texture> &texture)
    -> Error {
  ZoneScopedN("Shader::Send texture");

  [[unlikely]]
  if (!texture.isValid()) {
    auto keyname = Reflect::ResourceKeyToString(key);
    return Error::Createf("Texture {} is null.", keyname);
  }

  [[unlikely]]
  if (texture->view == VK_NULL_HANDLE) {
    auto keyname = Reflect::ResourceKeyToString(key);
    return Error::Createf("Texture {} has no valid image view.", keyname);
  }

  auto iter = reflection.keyToSlot.find(key);
  if (iter != reflection.keyToSlot.end()) {
    auto locationKey = iter->second;
    auto [set, binding] = Utils::SlotToSetBinding(locationKey);

    auto *info = GetSlotDescription(set, binding);
    auto *samplerInfo = std::get_if<Reflect::SamplerInfo>(&info->info);
    if (samplerInfo == nullptr) {
      auto keyname = Reflect::ResourceKeyToString(key);
      return Error::Createf(
          "Resource {} is not a sampler in shader reflection: {}", keyname,
          name);
    }

    GetState().userBoundTextures[locationKey] =
        BoundTexturePair{texture, samplerInfo};
    return Error::Success();
  }

  auto keyname = Reflect::ResourceKeyToString(key);
  return Error::Createf("Sampler {}, not found in shader reflection: {}",
                        keyname, name);
}

auto Shader::hash() const -> size_t {
  return reinterpret_cast<size_t>(module); // NOLINT
}

auto Shader::GetThreadgroupSize() const -> Result<Math::Uvec3> {
  [[unlikely]]
  if (threadgroupSize.x == 0 || threadgroupSize.y == 0 ||
      threadgroupSize.z == 0) {
    return Error::Unexpected(
        "Shader is not a compute shader or threadgroup size not set.");
  }
  return threadgroupSize;
}

auto Shader::GetWaveSize() const -> uint32_t { return waveSize; }

auto Shader::GetSlotDescription(uint32_t set, uint32_t binding) // NOLINT
    -> Reflect::ResourceInfo * {

  auto key = Utils::SetBindingToSlot(set, binding);

  auto iter = reflection.slotToInfo.find(key);
  if (iter == reflection.slotToInfo.end()) {
    return nullptr;
  }

  return &iter->second;
}

auto Shader::GetSlotDescription(uint64_t slot) -> Reflect::ResourceInfo * {
  auto iter = reflection.slotToInfo.find(slot);
  if (iter == reflection.slotToInfo.end()) {
    return nullptr;
  }

  return &iter->second;
}

} // namespace Graphics