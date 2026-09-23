#include "graphics.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/descriptorCache.hpp"
#include "Graphics/FrameGraph/pipelineCache.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/deviceSettings.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/render.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/shader.hpp"
#include "Libraries/vma.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/color.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/window.hpp"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_vulkan.h"

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Graphics {

GraphicsMutexes GraphicsContext::mutexes = {};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

GraphicsContext *g_ctx = nullptr;

thread_local std::string ContextDebugname{};

std::vector<VkCommandPool> CommandPools{};
std::mutex CommandPoolsMutex{};
SemaphoreManager semaphoreManager{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto GetDeferredDestructionAllowed() -> bool & {
  static bool deferredDestructionAllowed = true;
  return deferredDestructionAllowed;
}

static auto FindPhysicalDevice(GraphicsContext &context) -> Error {
  uint32_t gpuCount = 0;
  Error error = Error::Create(
      vkEnumeratePhysicalDevices(context.instance, &gpuCount, nullptr));

  if (gpuCount == 0) {
    return Error::Create("No Vulkan-compatible GPUs found.");
  }

  if (Error::IsError(error)) {
    return error;
  }

  std::vector<VkPhysicalDevice> gpus(gpuCount);
  error = Error::Create(
      vkEnumeratePhysicalDevices(context.instance, &gpuCount, gpus.data()));

  if (Error::IsError(error)) {
    return error;
  }

  // Keep a score of the best GPU found
  int bestGpuIndex = -1;
  int bestGpuScore = -1;

  const int DiscreteGPUScore = 1000;

  for (uint32_t i = 0; i < gpuCount; i++) {
    VkPhysicalDeviceProperties2 deviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };

    vkGetPhysicalDeviceProperties2(gpus.at(i), &deviceProperties);

    int score = 0;

    // Prefer discrete GPUs
    if (deviceProperties.properties.deviceType ==
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += DiscreteGPUScore;
    }

    // Higher max image dimension gets a higher score
    score += static_cast<int>(
        deviceProperties.properties.limits.maxImageDimension2D);

    if (score > bestGpuScore) {
      bestGpuScore = score;
      bestGpuIndex = static_cast<int>(i);
      context.deviceProperties = deviceProperties.properties;
    }
  }

  if (bestGpuIndex == -1) {
    return Error::Create("Failed to find a suitable GPU.");
  }

  context.physicalDevice = gpus.at(bestGpuIndex);

  VkPhysicalDeviceAccelerationStructurePropertiesKHR accelStructProperties{
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
  };

  VkPhysicalDeviceProperties2 deviceProperties2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &accelStructProperties,
  };

  vkGetPhysicalDeviceProperties2(context.physicalDevice, &deviceProperties2);

  context.accelerationStructureProperties = accelStructProperties;

  return Error::Success();
}

static auto FindQueueFamilies(GraphicsContext &context) -> Error {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice,
                                           &queueFamilyCount, nullptr);

  ERR_ASSERT(queueFamilyCount != 0);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(
      context.physicalDevice, &queueFamilyCount, queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, i,
                                         context.surface, &presentSupport);

    auto flags = queueFamilies.at(i).queueFlags;

    if (Utils::Includes(flags, VK_QUEUE_GRAPHICS_BIT)) {
      ERR_ASSERT(presentSupport);

      context.graphicsQueueFamily = i;
    } else if (Utils::Includes(flags, VK_QUEUE_COMPUTE_BIT)) {
      context.computeQueueFamily = i;
    } else if (Utils::Includes(flags, VK_QUEUE_TRANSFER_BIT)) {
      context.transferQueueFamily = i;
    }
  }

  ERR_ASSERT(context.graphicsQueueFamily != UINT32_MAX);

  return Error::Success();
}

auto GetAvailableDeviceExtensions(const GraphicsContext &context)
    -> Result<std::vector<VkExtensionProperties>> {
  uint32_t extensionCount = 0;
  CHECK_NEW_ERR(vkEnumerateDeviceExtensionProperties(
      context.physicalDevice, nullptr, &extensionCount, nullptr));

  std::vector<VkExtensionProperties> extensions(extensionCount);
  CHECK_NEW_ERR(vkEnumerateDeviceExtensionProperties(
      context.physicalDevice, nullptr, &extensionCount, extensions.data()));

  return extensions;
}

auto ExtensionListSupported(
    const std::vector<std::pair<const char *, ExtensionRequirement>>
        &extensions,
    const std::vector<VkExtensionProperties> &availableExtensions)
    -> std::vector<bool> {
  std::vector<bool> supported(extensions.size(), false);

  for (size_t i = 0; i < extensions.size(); i++) {
    for (const auto &available : availableExtensions) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay, hicpp-no-array-decay)
      if (strcmp(extensions.at(i).first, available.extensionName) == 0) {
        supported.at(i) = true;
        break;
      }
    }
  }

  return supported;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static auto CreateDevice(GraphicsContext &context,
                         const DeviceSettings &settings) -> Error {
  float queuePriority = 1.0F;
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

  if (context.graphicsQueueFamily != UINT32_MAX) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = context.graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.emplace_back(queueCreateInfo);
  }

  if (context.computeQueueFamily != UINT32_MAX) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = context.computeQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.emplace_back(queueCreateInfo);
  }

  if (context.transferQueueFamily != UINT32_MAX) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = context.transferQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.emplace_back(queueCreateInfo);
  }

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = queueCreateInfos.size();
  createInfo.pEnabledFeatures = nullptr;

  VkPhysicalDeviceIndexTypeUint8FeaturesEXT indexTypeUint8Features{};
  indexTypeUint8Features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
  indexTypeUint8Features.indexTypeUint8 = VK_TRUE;

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
  accelStructFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

  accelStructFeatures.accelerationStructure =
      settings.hardwareRaytracing == ExtensionRequirement::Required ? VK_TRUE
                                                                    : VK_FALSE;
  indexTypeUint8Features.pNext = &accelStructFeatures;

  VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
  rayQueryFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
  rayQueryFeatures.rayQuery =
      settings.inlineRaytracing == ExtensionRequirement::Required ? VK_TRUE
                                                                  : VK_FALSE;
  accelStructFeatures.pNext = &rayQueryFeatures;

  VkPhysicalDeviceVertexAttributeDivisorFeaturesKHR
      vertexAttributeDivisorProperties{};
  vertexAttributeDivisorProperties.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_KHR;
  vertexAttributeDivisorProperties.vertexAttributeInstanceRateDivisor = VK_TRUE;
  vertexAttributeDivisorProperties.pNext = &indexTypeUint8Features;

  VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dyn3{};
  dyn3.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
  dyn3.extendedDynamicState3ColorBlendEquation = VK_TRUE;
  dyn3.extendedDynamicState3ColorWriteMask = VK_TRUE;
  dyn3.extendedDynamicState3ColorBlendEnable = VK_TRUE;
  dyn3.pNext = &vertexAttributeDivisorProperties;

  VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT feature{};
  feature.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
  feature.vertexInputDynamicState = VK_TRUE;
  feature.pNext = &dyn3;

  // --- Vulkan 1.3 features ---
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &feature,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
  };

  PrintDebug("Enabled Vulkan 1.3 features: synchronization2, dynamicRendering");

  // --- Vulkan 1.2 features ---
  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &features13,
      .shaderFloat16 = VK_TRUE,
      .shaderInt8 = VK_TRUE,
      .descriptorIndexing = VK_TRUE,
      .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
      .shaderStorageBufferArrayNonUniformIndexing = VK_TRUE,
      .runtimeDescriptorArray = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
  };

  PrintDebug(
      "Enabled Vulkan 1.2 features: descriptorIndexing, "
      "shaderSampledImageArrayNonUniformIndexing, "
      "shaderStorageBufferArrayNonUniformIndexing, runtimeDescriptorArray, "
      "timelineSemaphore, bufferDeviceAddress");

  // --- Vulkan 1.1 features ---
  VkPhysicalDeviceVulkan11Features features11{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &features12,
      .shaderDrawParameters = VK_TRUE,
  };

  // --- Vulkan 1.0 features ---
  VkPhysicalDeviceFeatures features10{};
  features10.samplerAnisotropy = VK_TRUE;
  features10.textureCompressionBC = VK_TRUE;

  // --- Features2 root ---
  VkPhysicalDeviceFeatures2 features2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &features11,
      .features = features10,
  };

  // Device create info
  createInfo.pNext = &features2;

  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  constexpr auto extDisabled = ExtensionRequirement::Disabled;
  constexpr auto extOptional = ExtensionRequirement::Optional;
  constexpr auto extRequired = ExtensionRequirement::Required;

  const auto &availableExtensions =
      CHECK_RES(GetAvailableDeviceExtensions(context));

  auto extensions = std::vector<std::pair<const char *, ExtensionRequirement>>{
      {VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, extRequired},
      {VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME, extRequired},
      {VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME, extRequired},
      {VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME, extRequired},
      {VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, extOptional},
      {VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, extOptional},
      {VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME, extOptional},
      {VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, extRequired},
      {VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, extRequired},

      {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
       settings.hardwareRaytracing},
      {VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, settings.hardwareRaytracing},
      {VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
       settings.hardwareRaytracing},
      {VK_KHR_RAY_QUERY_EXTENSION_NAME, settings.inlineRaytracing},
  };

  const auto &supported =
      ExtensionListSupported(extensions, availableExtensions);
  bool allRequiredSupported = true;
  auto index = 0;

  for (const auto &[extensionName, requirement] : extensions) {
    if (supported.at(index) && requirement != extDisabled) {
      deviceExtensions.emplace_back(extensionName);
    } else {
      if (requirement == extRequired) {
        return Error::Create(std::format(
            "Required device extension not supported: {}", extensionName));
      }

      if (requirement == extOptional) {
        PrintWarning("Optional device extension not supported: {}",
                     extensionName);
      } else {
        PrintInfo("Device extension not supported (disabled): {}",
                  extensionName);
      }
    }
    index++;
  }

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  CHECK_NEW_ERR(vkCreateDevice(context.physicalDevice, &createInfo,
                               GetAllocationCallbacks(), &context.device));

  PrintDebug("Loading Vulkan device with Volk...");
  volkLoadDevice(context.device);

  PrintAlways("graphics: {}, compute: {}, transfer: {}",
              context.graphicsQueueFamily, context.computeQueueFamily,
              context.transferQueueFamily);

  context.queues.resize(
      std::max<size_t>(context.graphicsQueueFamily + 1, context.queues.size()));
  vkGetDeviceQueue(context.device, context.graphicsQueueFamily, 0,
                   &context.queues.at(context.graphicsQueueFamily));

  if (context.computeQueueFamily != UINT32_MAX) {
    context.queues.resize(std::max<size_t>(context.computeQueueFamily + 1,
                                           context.queues.size()));
    vkGetDeviceQueue(context.device, context.computeQueueFamily, 0,
                     &context.queues.at(context.computeQueueFamily));
  }

  if (context.transferQueueFamily != UINT32_MAX) {
    context.queues.resize(std::max<size_t>(context.transferQueueFamily + 1,
                                           context.queues.size()));
    vkGetDeviceQueue(context.device, context.transferQueueFamily, 0,
                     &context.queues.at(context.transferQueueFamily));
  }

  return Error::Success();
}

auto GetThreadContext() -> ThreadContext & {
  thread_local ThreadContext threadContext;
  return threadContext;
}

// May be null
auto GetVirtualCommandBuffer() -> VirtualCommandBuffer * {
  const auto &threadContext = GetThreadContext();
  return threadContext.commandBuffer.get();
}

auto GetVkCommandBuffer() -> VkCommandBuffer {
  const auto &threadContext = GetThreadContext();
  return threadContext.workingCommandBuffer;
}

auto PushDebugMarker(const std::string_view &name, const Color *color) -> void {
  auto *cmdBuffer = GetVirtualCommandBuffer();
  if (cmdBuffer == nullptr) {
    PrintWarning("PushDebugMarker called with null command buffer.");
    return;
  }

  VkDebugUtilsLabelEXT labelInfo{};
  labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  labelInfo.pLabelName = name.data();

  // NOLINTBEGIN
  if (color == nullptr) {
    Color::RandomColor(name).FillFloatArray(labelInfo.color);
  } else {
    color->FillFloatArray(labelInfo.color);
  }
  // NOLINTEND

  cmdBuffer->BeginDebugUtilsLabelEXT(
      Args::VkCmdBeginDebugUtilsLabelEXT{&labelInfo});
}

auto PopDebugMarker() -> void {
  auto *cmdBuffer = GetVirtualCommandBuffer();
  if (cmdBuffer == nullptr) {
    PrintWarning("PopDebugMarker called with null command buffer.");
    return;
  }

  cmdBuffer->EndDebugUtilsLabelEXT({});
}

auto PushDebugLabel(const std::string_view &name, const Color *color) -> void {
  auto *cmdBuffer = GetVirtualCommandBuffer();
  if (cmdBuffer == nullptr) {
    PrintWarning("PushDebugLabel called with null command buffer.");
    return;
  }

  VkDebugUtilsLabelEXT labelInfo{};
  labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  labelInfo.pLabelName = name.data();

  // NOLINTBEGIN
  if (color == nullptr) {
    Color::RandomColor(name).FillFloatArray(labelInfo.color);
  } else {
    color->FillFloatArray(labelInfo.color);
  }
  // NOLINTEND

  cmdBuffer->InsertDebugUtilsLabelEXT(
      Args::VkCmdInsertDebugUtilsLabelEXT{&labelInfo});
}

static auto CreateSemaphores(GraphicsContext &context) -> Error {
  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  context.imageAvailable.resize(FRAMES_IN_FLIGHT);
  context.inFlight.resize(FRAMES_IN_FLIGHT);

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
      CHECK_NEW_ERR(vkCreateSemaphore(context.device, &semaphoreInfo,
                                      GetAllocationCallbacks(),
                                      &context.imageAvailable.at(i)));

      CHECK_NEW_ERR(vkCreateFence(context.device, &fenceInfo,
                                  GetAllocationCallbacks(),
                                  &context.inFlight.at(i)));
    }
  }

  return Error::Success();
}

static auto CreateVmaAllocator(GraphicsContext &context) -> Error {
  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  VmaAllocatorCreateInfo allocatorInfo = {0};
  allocatorInfo.physicalDevice = context.physicalDevice;
  allocatorInfo.device = context.device;
  allocatorInfo.instance = context.instance;
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;
  allocatorInfo.pAllocationCallbacks = GetAllocationCallbacks();
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
                        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaVulkanFunctions vulkanFunctions;
  CHECK_NEW_ERR(
      vmaImportVulkanFunctionsFromVolk(&allocatorInfo, &vulkanFunctions));

  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  CHECK_NEW_ERR(vmaCreateAllocator(&allocatorInfo, &context.vmaAllocator));

  return Error::Success();
}

inline auto CreateCommandPool(ThreadContext &tcontext) -> Error {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = tcontext.queueFamily;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateCommandPool(tcontext.graphicsContext->device,
                                      &poolInfo, GetAllocationCallbacks(),
                                      &tcontext.commandPool));
  }

  {
    std::lock_guard<std::mutex> lock(CommandPoolsMutex);
    CommandPools.emplace_back(tcontext.commandPool);
  }

  return Error::Success();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Initialize(GraphicsContext &context, Window::WindowContext &wcontext,
                const DeviceSettings &deviceSettings) -> Error {

  GlobalAllocations.RegisterNewThreadAllocations();

  PrintDebug("Initializing Volk...");
  CHECK_NEW_ERR(volkInitialize());

  // Initialize SDL for Vulkan
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    return Error::Create(SDL_GetError());
  }

  SDL_Window *window = CHECK_NULL(SDL_CreateWindow(
      wcontext.initialSettings.title.c_str(), wcontext.initialSettings.width,
      wcontext.initialSettings.height,
      SDL_WINDOW_VULKAN | wcontext.initialSettings.GetSDLWindowFlags()));

  context.sdlWindow = window;

  Window::SetSettings(wcontext, wcontext.initialSettings);

  // Get vulkan instance extensions required by SDL
  unsigned int extensionCount = 0;
  SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  ERR_ASSERT(extensionCount != 0)

  Uint32 extCount = 0;
  const char *const *extensions =
      CHECK_NULL(SDL_Vulkan_GetInstanceExtensions(&extCount));

  std::vector<const char *> extensionList;

  extensionList.reserve(extCount);
  for (Uint32 i = 0; i < extCount; i++) {
    extensionList.emplace_back(extensions[i]); // NOLINT
  }

  extensionList.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Snap Engine";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Snap";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_4;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(extensionList.size());
  createInfo.ppEnabledExtensionNames = extensionList.data();
  createInfo.pApplicationInfo = &appInfo;

  CHECK_NEW_ERR(vkCreateInstance(&createInfo, GetAllocationCallbacks(),
                                 &context.instance));

  PrintDebug("Loading Vulkan instance with Volk...");
  // Load instance-level Vulkan functions using Volk
  volkLoadInstance(context.instance);

  // Create Vulkan surface for the SDL window
  ERR_ASSERT(SDL_Vulkan_CreateSurface(window, context.instance, nullptr,
                                      &context.surface));

  CHECK_ERR(FindPhysicalDevice(context));
  CHECK_ERR(FindQueueFamilies(context));

  auto *windowContext = CHECK_NULL(Window::GetWindowContext());

  CHECK_ERR(CreateDevice(context, deviceSettings));
  PrintDebug("called: CreateDevice...");
  CHECK_ERR(CreateVmaAllocator(context));
  PrintDebug("called: CreateVmaAllocator...");

  GetThreadContext().graphicsContext = &context;

  CHECK_ERR(CreateCommandPool(GetThreadContext()));
  PrintDebug("called: CreateCommandPool...");
  CHECK_ERR(CreateSemaphores(context));
  PrintDebug("called: CreateSemaphores...");
  CHECK_ERR(GetPipelineCache().Initialize(context));
  CHECK_ERR(GetDescriptorCache().Initialize(context));

  return Error::Success();
}

void Deinitialize(GraphicsContext &context) {
  PrintInfo("Deinitializing graphics context...");

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkDeviceWaitIdle(context.device);
  }

  Graphics::UploadBuffers.clear();

  // Graphics::Barrier::ResetModule();
  Graphics::UnloadShaderModule(context);
  Graphics::DeinitializeRendering(context);
  Graphics::RenderState::Shutdown(context);
  Graphics::semaphoreManager.Deinitialize(context);
  Graphics::DestroySamplers(context);
  Graphics::RenderState::Destroy(context);
  Graphics::DeInitializeBVHModule();
  GetPipelineCache().DeInitialize(context);
  GetDescriptorCache().DeInitialize(context);

  // (BEFORE device, allocator lock)
  Graphics::ProcessReleasedResources(context);

  std::scoped_lock<std::mutex, std::mutex> lock(
      Graphics::GraphicsContext::mutexes.device,
      Graphics::GraphicsContext::mutexes.vmaAllocator);

  for (auto &descriptorPoolInfo : GetThreadContext().descriptorPools) {
    vkDestroyDescriptorPool(context.device, descriptorPoolInfo.descriptorPool,
                            GetAllocationCallbacks());
  }

  vmaDestroyAllocator(context.vmaAllocator);

  for (VkSemaphore semaphore : context.imageAvailable) {
    vkDestroySemaphore(context.device, semaphore, GetAllocationCallbacks());
  }

  for (VkFence fence : context.inFlight) {
    vkDestroyFence(context.device, fence, GetAllocationCallbacks());
  }

  {
    std::lock_guard<std::mutex> lock(CommandPoolsMutex);
    for (auto &pool : CommandPools) {
      vkDestroyCommandPool(context.device, pool, GetAllocationCallbacks());
    }
    CommandPools.clear();
  }

  vkDestroyDevice(context.device, GetAllocationCallbacks());
  vkDestroySurfaceKHR(context.instance, context.surface,
                      GetAllocationCallbacks());
  vkDestroyInstance(context.instance, GetAllocationCallbacks());

  SDL_DestroyWindow(context.sdlWindow);
  SDL_Quit();

  context.sdlWindow = nullptr;
}

auto BeginSingleTimeCommands(const GraphicsContext &context)
    -> VkCommandBuffer {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  auto &tcontext = GetThreadContext();

  allocInfo.commandPool = tcontext.commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer = nullptr;
  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
    vkAllocateCommandBuffers(context.device, &allocInfo, &commandBuffer);
  }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

auto EndSingleTimeCommands(const GraphicsContext &context,
                           VkCommandBuffer commandBuffer) -> void {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  auto &tcontext = GetThreadContext();

  vkQueueSubmit(context.queues.at(tcontext.queueFamily), 1, &submitInfo,
                VK_NULL_HANDLE);
  vkQueueWaitIdle(context.queues.at(tcontext.queueFamily));

  vkFreeCommandBuffers(context.device, tcontext.commandPool, 1, &commandBuffer);
}

void SetCurrentGraphicsContext(GraphicsContext *ctx) { g_ctx = ctx; }
auto GetCurrentGraphicsContext() -> GraphicsContext * { return g_ctx; }

} // namespace Graphics