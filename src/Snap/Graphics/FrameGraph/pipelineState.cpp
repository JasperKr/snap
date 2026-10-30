#include "Graphics/FrameGraph/pipelineState.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/FrameGraph/recordingState.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/descriptorHeap.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/renderState.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include <cassert>
#include <format>
#include <mutex>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

auto Reset(const GraphicsContext &context) -> Error {
  RecordingState::CurrentState = {};
  RecordingState::LastState = nullptr;

  return Error::Success();
}

void Shutdown(const GraphicsContext &context) {
  RecordingState::LastState = nullptr;
  RecordingState::LastStateStorage = {};
}

auto IsSwapchainTexture(const GraphicsContext &context,
                        const Graphics::Texture &texture) -> bool {
  for (const auto &swapchainTexture : context.swapchainInfo.textures) {
    if (swapchainTexture.get() == &texture) {
      return true;
    }
  }

  return false;
}

auto Destroy(const GraphicsContext &context) -> void {}

auto BeginFrame(const GraphicsContext &context) -> Error {
  RecordingState::CurrentState = {};
  RecordingState::LastState = nullptr;
  RecordingState::LastStateStorage = {};

  return Error::Success();
}

auto inline GetInputAssemblyState(const RecordingState::CombinedState &state)
    -> VkPipelineInputAssemblyStateCreateInfo {
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};

  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = state.renderState->primitiveTopology;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  return inputAssembly;
}

auto inline GetRasterizationState(const RecordingState::CombinedState &state)
    -> VkPipelineRasterizationStateCreateInfo {
  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = state.renderState->polygonMode;
  rasterizer.lineWidth = 1.0F;
  rasterizer.cullMode = state.cullMode;
  rasterizer.frontFace = state.frontFace;
  rasterizer.depthBiasEnable = VK_FALSE;

  return rasterizer;
}

auto inline GetColorBlendAttachmentState(
    const GraphicsContext &context, const RecordingState::CombinedState &state)
    -> Result<std::vector<VkPipelineColorBlendAttachmentState>> {

  // Get Shader Output Reflection //

  auto *shader = state.renderState->shader.get();

  // #ifndef NDEBUG
  //   if (!shader->entryPointToStageIndex.contains(
  //           SlangStage::SLANG_STAGE_FRAGMENT)) {
  //     return std::vector<VkPipelineColorBlendAttachmentState>{};
  //   }

  //   auto entryPointIndex =
  //       shader->entryPointToStageIndex.at(SlangStage::SLANG_STAGE_FRAGMENT);

  //   auto *entryPoint =
  //       shader->programLayout->getEntryPointByIndex(entryPointIndex);

  //   if (entryPoint == nullptr) {
  //     return Error::Unexpected(
  //         "Failed to get fragment entry point from shader program layout");
  //   }

  //   auto *outputVariableLayout = entryPoint->getResultVarLayout();

  //   if (outputVariableLayout == nullptr) {
  //     // No fragment outputs, so no blend attachments needed
  //     return std::vector<VkPipelineColorBlendAttachmentState>{};
  //   }

  //   // Determine Expected Output Attachments //

  //   std::unordered_set<uint32_t> expectedAttachments = {};
  //   for (uint32_t i = 0;
  //        i < outputVariableLayout->getTypeLayout()->getFieldCount(); ++i) {
  //     auto *outVar = outputVariableLayout->getTypeLayout()->getFieldByIndex(i);
  //     if (outVar->getSemanticName() != nullptr &&
  //         strcmp(outVar->getSemanticName(), "SV_Target") == 0) {
  //       expectedAttachments.insert(outVar->getSemanticIndex());
  //     }
  //   }
  // #endif

  // Get Actual Set Render Targets //

  auto blendAttachments = std::vector<VkPipelineColorBlendAttachmentState>();
  blendAttachments.resize(MAX_COLOR_ATTACHMENTS);

  bool hasImplicitLocation = false;
  bool hasExplicitLocation = false;

  for (int i = 0; i < state.renderState->colorAttachments.size(); ++i) {
    const auto &rendertarget = state.renderState->colorAttachments.at(i);
    int location = rendertarget.location;
    if (location == -1) {
      location = i;
      hasImplicitLocation = true;
    } else {
      hasExplicitLocation = true;
    }

    if (location < 0 || location >= MAX_COLOR_ATTACHMENTS) {
      return Error::Unexpected("Render target location is out of bounds: " +
                               std::to_string(location));
    }

    blendAttachments.at(location) = rendertarget.blendMode;
  }

  if (hasImplicitLocation && hasExplicitLocation) {
    return Error::Unexpected(
        "Cannot mix implicit and explicit render target locations.");
  }

  // #ifndef NDEBUG
  //   for (uint32_t i = 0; i <= blendAttachments.size(); ++i) {
  //     if (expectedAttachments.contains(i)) {
  //       if (i >= blendAttachments.size()) {
  //         return Error::Unexpected(
  //             "Missing blend attachment for expected output location " +
  //             std::to_string(i));
  //       }
  //     }
  //   }
  // #endif

  return blendAttachments;
}

auto GetTargetSize() -> VkExtent2D {
  if (RecordingState::CurrentState.renderState->colorAttachments.size() > 0) {
    return {
        RecordingState::CurrentState.renderState->colorAttachments.at(0)
            .texture->GetWidth(),
        RecordingState::CurrentState.renderState->colorAttachments.at(0)
            .texture->GetHeight(),
    };
  }

  if (RecordingState::CurrentState.renderState->hasDepthStencilAttachment) {
    return {
        RecordingState::CurrentState.renderState->depthStencilAttachment
            .texture->GetWidth(),
        RecordingState::CurrentState.renderState->depthStencilAttachment
            .texture->GetHeight(),
    };
  }

  // No attachments, return zero size
  return {0, 0};
}

inline auto CreateGraphicsPipeline(const GraphicsContext &context,
                                   RecordingState::CombinedState &state)
    -> Result<VkPipeline> {
  ZoneScoped;
  if (state.renderState->bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return Error::Unexpected(
        "Attempted to create graphics pipeline with non-graphics bind point.");
  }

  ERR_ASSERT_MSG(
      state.renderState->shader->combinedShaderStages !=
          VK_SHADER_STAGE_COMPUTE_BIT,
      std::format("Shader: {}, Is being bound to a graphics pipeline.",
                  state.renderState->shader->moduleName));

  PrintDebug("Creating graphics pipeline");

  VkShaderDescriptorSetAndBindingMappingInfoEXT globalUBOMappingInfo{};
  globalUBOMappingInfo.mappingCount = 1;
  globalUBOMappingInfo.sType =
      VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT;
  globalUBOMappingInfo.pNext = nullptr;

  VkDescriptorSetAndBindingMappingEXT globalUBOMapping{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
      .pNext = nullptr,
      .descriptorSet = state.renderState->shader->reflection.globals.set,
      .firstBinding = state.renderState->shader->reflection.globals.binding,
      .bindingCount = 1,
      .resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT,
      .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
      .sourceData{
          .pushIndex{
              .heapOffset = 0,
              .pushOffset = 0,
              .heapIndexStride = static_cast<uint32_t>(
                  DescriptorHeapManager::GetInstance()
                      .heapProperties.bufferDescriptorSize),
              .heapArrayStride = 0,
          },
      },
  };

  globalUBOMappingInfo.pMappings = &globalUBOMapping;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};

  assert(state.renderState->shader.get() != nullptr);

  for (const auto &stage : state.renderState->shader->entryPoints) {
    auto &stageInfo = shaderStages.emplace_back();

    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage.second;
    stageInfo.module = state.renderState->shader->module;
    stageInfo.pName = stage.first.c_str();
    stageInfo.pNext = &globalUBOMappingInfo;
  }

  auto inputAssembly = GetInputAssemblyState(state);
  auto rasterizer = GetRasterizationState(state);

  /// Viewport and Scissor ///

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

  viewportState.viewportCount = 1;
  viewportState.pViewports = &state.viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &state.scissor;

  /// Multisampling ///

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  /// Dynamic State ///

  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

  // NOLINTNEXTLINE
  std::array<VkDynamicState, 11> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
      VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,

      VK_DYNAMIC_STATE_CULL_MODE,
      VK_DYNAMIC_STATE_FRONT_FACE,

      VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
      VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
      VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,

      VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
      VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
      VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
  };

  dynamicState.pDynamicStates = dynamicStates.data();
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());

  /// Depth and Stencil ///

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable = state.depthTestEnable;
  depthStencilState.depthWriteEnable = state.depthWriteEnable;
  depthStencilState.depthCompareOp = state.depthCompareOp;
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.stencilTestEnable = state.renderState->stencilTestEnable;

  /// Attachment Formats ///

  auto formats = std::array<VkFormat, MAX_COLOR_ATTACHMENTS>();
  VkFormat depthFormat = VK_FORMAT_UNDEFINED;
  VkFormat stencilFormat = VK_FORMAT_UNDEFINED;

  if (state.renderState->hasDepthStencilAttachment) {
    const auto &rendertarget = state.renderState->depthStencilAttachment;

    if (rendertarget.texture->IsDepthTexture()) {
      depthFormat = rendertarget.texture->GetFormat();
    }
    if (rendertarget.texture->IsStencilTexture()) {
      stencilFormat = rendertarget.texture->GetFormat();
    }
  }

  bool hasImplicitLocation = false;
  bool hasExplicitLocation = false;

  for (int i = 0; i < state.renderState->colorAttachments.size(); i++) {
    const auto &rendertarget = state.renderState->colorAttachments.at(i);

    int location = rendertarget.location;
    if (location == -1) {
      location = i;
      hasImplicitLocation = true;
    } else {
      hasExplicitLocation = true;
    }

    formats.at(location) = rendertarget.texture->GetFormat();
  }

  if (hasImplicitLocation && hasExplicitLocation) {
    return Error::Unexpected(
        "Cannot mix explicit and implicit render target locations.");
  }

  /// Color Attachments ///

  VkPipelineRenderingCreateInfo renderingCreateInfo = {};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount =
      state.renderState->colorAttachments.size();
  renderingCreateInfo.pColorAttachmentFormats = formats.data();
  renderingCreateInfo.depthAttachmentFormat = depthFormat;
  renderingCreateInfo.stencilAttachmentFormat = stencilFormat;

  auto blendAttachments =
      CHECK_RES(GetColorBlendAttachmentState(context, state));

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = state.renderState->colorAttachments.size();
  colorBlending.pAttachments = blendAttachments.data();

  if (state.renderState->colorAttachments.size() == 0) {
    colorBlending.pAttachments = nullptr;
  }

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  pipelineInfo.pStages = shaderStages.data();
  pipelineInfo.pVertexInputState = nullptr; // Dynamic vertex input state
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &depthStencilState;
  pipelineInfo.pDynamicState = &dynamicState;

  auto *shader = state.renderState->shader.get();

  VkPipelineCreateFlags2CreateInfo pipelineCreateFlags2CI{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
      .pNext = &renderingCreateInfo,
      .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
  };

  pipelineInfo.layout = VK_NULL_HANDLE;
  pipelineInfo.renderPass = VK_NULL_HANDLE; // Not needed
  pipelineInfo.subpass = 0;
  pipelineInfo.pNext = &pipelineCreateFlags2CI;

  PrintDebug("Creating graphics pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateGraphicsPipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
        GetAllocationCallbacks(), &pipeline));
  }

  {
    std::lock_guard<std::mutex> lock(GetPipelineCache().mutex);

    auto key = StateKey(*state.renderState);
    GetPipelineCache().cache.emplace(key, pipeline);
    GetPipelineCache().pipelines.emplace_back(pipeline);
  }

  return pipeline;
}

inline auto CreateComputePipeline(const GraphicsContext &context,
                                  RecordingState::CombinedState &state)
    -> Result<VkPipeline> {
  ZoneScoped;

  ERR_ASSERT(state.renderState->shader->combinedShaderStages ==
             VK_SHADER_STAGE_COMPUTE_BIT);

  PrintDebug("Creating compute pipeline");

  VkShaderDescriptorSetAndBindingMappingInfoEXT globalUBOMappingInfo{};
  globalUBOMappingInfo.mappingCount = 1;
  globalUBOMappingInfo.sType =
      VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT;
  globalUBOMappingInfo.pNext = nullptr;

  VkDescriptorSetAndBindingMappingEXT globalUBOMapping{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT,
      .pNext = nullptr,
      .descriptorSet = state.renderState->shader->reflection.globals.set,
      .firstBinding = state.renderState->shader->reflection.globals.binding,
      .bindingCount = 1,
      .resourceMask = VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT,
      .source = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT,
      .sourceData{
          .pushIndex{
              .heapOffset = 0,
              .pushOffset = 0,
              .heapIndexStride = static_cast<uint32_t>(
                  DescriptorHeapManager::GetInstance()
                      .heapProperties.bufferDescriptorSize),
              .heapArrayStride = 0,
          },
      },
  };

  globalUBOMappingInfo.pMappings = &globalUBOMapping;
  std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};

  assert(state.renderState->shader.get() != nullptr);

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  pipelineInfo.stage.module = state.renderState->shader->module;
  pipelineInfo.stage.pNext = &globalUBOMappingInfo;

  if ((state.renderState->shader->combinedShaderStages &
       VK_SHADER_STAGE_COMPUTE_BIT) == 0) {
    return Error::Unexpectedf(
        "Shader module {} does not have a compute shader stage.",
        state.renderState->shader->name);
  }

  // If we get a validation error / crash about this not existing; rename to "main"
  pipelineInfo.stage.pName = "computeMain";
  pipelineInfo.layout = VK_NULL_HANDLE;

  VkPipelineCreateFlags2CreateInfo pipelineCreateFlags2CI{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
  };

  pipelineInfo.pNext = &pipelineCreateFlags2CI;

  PrintDebug("Creating compute pipeline...");

  VkPipeline pipeline = VK_NULL_HANDLE;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateComputePipelines(
        context.device, VK_NULL_HANDLE, 1, &pipelineInfo,
        GetAllocationCallbacks(), &pipeline));
  }

  {
    std::lock_guard<std::mutex> lock(GetPipelineCache().mutex);

    auto key = StateKey(*state.renderState);
    GetPipelineCache().cache.emplace(key, pipeline);
    GetPipelineCache().pipelines.emplace_back(pipeline);
  }

  return pipeline;
}

inline auto CreatePipeline(const GraphicsContext &context,
                           RecordingState::CombinedState &state)
    -> Result<VkPipeline> {
  ZoneScoped;

  PrintDebug("Creating pipeline");

  assert(state.renderState->shader.get() != nullptr);

  if (state.renderState->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    return CreateGraphicsPipeline(context, state);
  }
  if (state.renderState->bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE) {
    return CreateComputePipeline(context, state);
  }

  return Error::Unexpected("Unsupported pipeline bind point.");
}

inline auto GetPipeline(const GraphicsContext &context,
                        RecordingState::CombinedState &state)
    -> Result<VkPipeline> {
  ZoneScoped;

  PrintDebug("Getting pipeline");
  assert(state.renderState->shader.get() != nullptr);

  {
    std::lock_guard<std::mutex> lock(GetPipelineCache().mutex);
    auto *pipeline = GetPipelineCache().cache.get(StateKey(*state.renderState));

    if (pipeline != nullptr) {
      return *pipeline;
    }
  }

  return CreatePipeline(context, state);
}

auto FlushCompute(const GraphicsContext &context,
                  VkCommandBuffer vkCommandBuffer) -> Result<bool> {
  ZoneScoped;

  PrintDebug("Flushing compute");

  if (RecordingState::CurrentState.renderState->bindPoint !=
      VK_PIPELINE_BIND_POINT_COMPUTE) {
    return Error::Unexpected("Current state is not a compute pipeline.");
  }

  VkPipeline pipeline =
      CHECK_RES(GetPipeline(context, RecordingState::CurrentState));

  assert(RecordingState::CurrentState.renderState->shader->entryPoints.at(0)
             .second == VK_SHADER_STAGE_COMPUTE_BIT);

  PrintDebug("Binding pipeline");

  vkCmdBindPipeline(vkCommandBuffer,
                    RecordingState::CurrentState.renderState->bindPoint,
                    pipeline);

  return true;
}

auto FlushGraphics(const GraphicsContext &context,
                   VkCommandBuffer vkCommandBuffer) -> Result<bool> {
  ZoneScoped;

  ERR_ASSERT(RecordingState::CurrentState.renderState->bindPoint ==
             VK_PIPELINE_BIND_POINT_GRAPHICS);

  VkPipeline pipeline =
      CHECK_RES(GetPipeline(context, RecordingState::CurrentState));

  PrintDebug("Binding pipeline");

  {
    ZoneScopedN("vkCmdBindPipeline");
    vkCmdBindPipeline(vkCommandBuffer,
                      RecordingState::CurrentState.renderState->bindPoint,
                      pipeline);
  }

  return true;
}

auto Flush(const GraphicsContext &context, VkCommandBuffer vkCommandBuffer)
    -> Result<bool> {
  ZoneScoped;

  PrintDebug("Flush");

  [[likely]]
  if (!Graphics::GetIsStateDirty() && RecordingState::LastState != nullptr &&
      RecordingState::CurrentState.renderState->GetHash() ==
          RecordingState::LastState->renderState->GetHash() &&
      *RecordingState::CurrentState.renderState ==
          *RecordingState::LastState->renderState) {
    return false;
  }

  assert(RecordingState::CurrentState.renderState->shader.get() != nullptr);

  RecordingState::LastStateStorage =
      RecordingState::CurrentState; // Copy current state to last state storage
  RecordingState::LastState =
      &RecordingState::LastStateStorage; // Point last state to the storage

  if (RecordingState::CurrentState.renderState->bindPoint ==
      VK_PIPELINE_BIND_POINT_GRAPHICS) {
    auto result = FlushGraphics(context, vkCommandBuffer);
    Graphics::GetIsStateDirty() = false;

    return result;
  }
  if (RecordingState::CurrentState.renderState->bindPoint ==
      VK_PIPELINE_BIND_POINT_COMPUTE) {
    auto result = FlushCompute(context, vkCommandBuffer);
    Graphics::GetIsStateDirty() = false;

    return result;
  }

  return Error::Unexpectedf(
      "Unsupported pipeline bind point in Flush: {}.",
      (int)RecordingState::CurrentState.renderState->bindPoint);
}

} // namespace Graphics