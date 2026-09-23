#include "render.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/FrameGraph/framegraph.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/renderThread.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Graphics/swapchainManager.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "graphics.hpp"
#include "renderState.hpp"
#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>

#include "vulkan/vulkan_core.h"
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../external/tracy/public/tracy/Tracy.hpp"

namespace Graphics {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StitchInfo GlobalStitchInfo{};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SwapchainManager::SwapchainManager swapchainManager;

static void ResetCommandBuffers(Graphics::GraphicsContext &context) {
  auto &cmdBuffers = GlobalStitchInfo.commandBuffers.at(context.frameIndex);
  for (auto &cmdBuffer : cmdBuffers) {
    vkResetCommandBuffer(cmdBuffer, 0);
  }
}

static auto StartRecording(Graphics::GraphicsContext &context) -> Error {
  if (GlobalStitchInfo.commandBuffers.at(context.frameIndex).empty()) {
    PrintDebug("Allocating stitch command buffer for frame {}",
               context.frameIndex);
    // Allocate 1 command buffer if none exist yet
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = GetThreadContext().commandPool;
    allocInfo.commandBufferCount = 1;

    GlobalStitchInfo.commandBuffers.at(context.frameIndex).resize(1);
    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);
      CHECK_NEW_ERR(vkAllocateCommandBuffers(
          context.device, &allocInfo,
          &GlobalStitchInfo.commandBuffers.at(context.frameIndex).at(0)));
    }
  }

  GetThreadContext().commandBuffer = nullptr;

  return Error::Success();
}

static auto EndRecording(Graphics::GraphicsContext &context,
                         uint32_t frameIndex) -> Error {
  ZoneScoped;
  Graphics::ProcessReleasedResources(context);

  return Error::Success();
}

auto AcquireNextSwapchainImage(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

  if (context.inFlight[context.frameIndex] != VK_NULL_HANDLE) {
    ZoneScopedN("Wait for in-flight fence");
    CHECK_NEW_ERR(vkWaitForFences(context.device, 1,
                                  &context.inFlight[context.frameIndex],
                                  VK_TRUE, UINT64_MAX));

    CHECK_NEW_ERR(vkResetFences(context.device, 1,
                                &context.inFlight[context.frameIndex]));
  }

  {
    ZoneScopedN("Acquire next image");
    auto res = (vkAcquireNextImageKHR(
        context.device, context.swapchainInfo.swapchain, UINT64_MAX,
        context.imageAvailable[context.frameIndex], VK_NULL_HANDLE,
        &context.swapchainImageIndex));

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
      // Swapchain is out of date, need to recreate
      swapchainManager.MakeDirty();
    }

    CHECK_NEW_ERR(res);
  }

  return Error::Success();
}

auto PrepareRecording(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;
  ResetCommandBuffers(context);

  CHECK_ERR(StartRecording(context));

  return Error::Success();
}

auto PresentFrame(Graphics::GraphicsContext &context) -> Error {
  ZoneScoped;

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores =
      &context.renderFinished.at(context.swapchainImageIndex);
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &context.swapchainInfo.swapchain;
  presentInfo.pImageIndices = &context.swapchainImageIndex;

  auto &tcontext = GetThreadContext();

  // Present the image
  CHECK_NEW_ERR(vkQueuePresentKHR(
      context.queues.at(context.graphicsQueueFamily), &presentInfo));

  return {};
}

auto InitializeRendering(Graphics::GraphicsContext &context,
                         Window::WindowContext &windowContext) -> Error {
  ZoneScoped;

  auto swapchainManagerResult =
      SwapchainManager::SwapchainManager::Initialize(context, windowContext);
  if (Error::IsError(swapchainManagerResult)) {
    return swapchainManagerResult.error();
  }

  swapchainManager = swapchainManagerResult.value();

  CHECK_ERR(AcquireNextSwapchainImage(context));

  CHECK_ERR(StartRecording(context));

  return Error::Success();
}

auto DeinitializeRendering(GraphicsContext &context) -> void {
  swapchainManager.Deinitialize(context);
}

struct CmdBufferSubmitInfo {
  VkCommandBuffer buffer;
  uint32_t queueFamily;
};

inline auto
SubmitCommandBuffers(Graphics::GraphicsContext &context,
                     const std::vector<CmdBufferSubmitInfo> &buffers) -> Error {
  ZoneScoped;

  assert(buffers.size() == 1);

  std::vector<VkCommandBufferSubmitInfo> commandBufferInfos(buffers.size());
  for (size_t i = 0; i < buffers.size(); i++) {
    commandBufferInfos.at(i).sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfos.at(i).commandBuffer = buffers.at(i).buffer;
  }

  VkSemaphoreSubmitInfo waitInfo = {};
  waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  waitInfo.semaphore = context.imageAvailable[context.frameIndex];
  waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSemaphoreSubmitInfo signalInfo = {};
  signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

  assert(context.renderFinished.size() > context.swapchainImageIndex);

  signalInfo.semaphore = context.renderFinished.at(context.swapchainImageIndex);
  signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

  VkSubmitInfo2 submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submitInfo.waitSemaphoreInfoCount = 1;
  submitInfo.pWaitSemaphoreInfos = &waitInfo;
  submitInfo.commandBufferInfoCount =
      static_cast<uint32_t>(commandBufferInfos.size());
  submitInfo.pCommandBufferInfos = commandBufferInfos.data();
  submitInfo.signalSemaphoreInfoCount = 1;
  submitInfo.pSignalSemaphoreInfos = &signalInfo;

  auto &tcontext = GetThreadContext();

  // PrintAlways("Queue family: {}", tcontext.queueFamily);

  {
    ZoneScopedN("Submit command buffer to queue");
    CHECK_NEW_ERR(vkQueueSubmit2(context.queues.at(buffers.front().queueFamily),
                                 1, &submitInfo,
                                 context.inFlight[context.frameIndex]));
  }

  auto timelineValue =
      CHECK_RES(Graphics::semaphoreManager.UpdateSemaphoreValues(context));

  {
    ZoneScopedN("Submit timeline semaphore signal");

    VkSemaphore globalTimelineSemaphore = Graphics::semaphoreManager.semaphore;
    if (globalTimelineSemaphore != VK_NULL_HANDLE) {
      VkSemaphoreSubmitInfo timelineSignalInfo = {};
      timelineSignalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
      timelineSignalInfo.semaphore = globalTimelineSemaphore;
      timelineSignalInfo.value = timelineValue;
      timelineSignalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

      VkSubmitInfo2 submitTimeline = {};
      submitTimeline.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
      submitTimeline.signalSemaphoreInfoCount = 1;
      submitTimeline.pSignalSemaphoreInfos = &timelineSignalInfo;

      CHECK_NEW_ERR(
          vkQueueSubmit2(context.queues.at(context.graphicsQueueFamily), 1,
                         &submitTimeline, VK_NULL_HANDLE));
    }
  }

  return Error::Success();
}

auto Present(Graphics::GraphicsContext &context,
             const std::vector<Ref<Threading::RenderThreadInfo>> &commands)
    -> Error {
  ZoneScoped;

  context.currentlyReordering = true;

  // Match and combine by queue family
  std::unordered_map<uint8_t, std::shared_ptr<VirtualCommandBuffer>> combined;
  for (const auto &command : commands) {
    auto &commandBuffer = command->threadData.commandBuffer;

    auto iter = combined.find(commandBuffer->GetQueueFamily());
    if (iter == combined.end()) {
      combined.emplace(commandBuffer->GetQueueFamily(), commandBuffer);
    } else {
      CHECK_ERR(iter->second->Append(*commandBuffer));
    }

    command->threadData.commandBuffer = nullptr;
  }

  std::vector<uint64_t> orderedSemaphoreValues = {};

  orderedSemaphoreValues.reserve(commands.size());
  for (const auto &command : commands) {
    orderedSemaphoreValues.emplace_back(
        command->threadData.cmdBufferTimelineValue);
  }

  Graphics::semaphoreManager.QueueTimelineValues(orderedSemaphoreValues);

  static FrameGraph graph;

  auto &availableCommandBuffers =
      GlobalStitchInfo.commandBuffers.at(context.frameIndex);

  if (availableCommandBuffers.size() < combined.size()) {
    auto allocationSize = combined.size() - availableCommandBuffers.size();
    auto previousSize = availableCommandBuffers.size();
    availableCommandBuffers.resize(combined.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = GetThreadContext().commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(allocationSize);

    auto *writeAddress =
        availableCommandBuffers.data() + previousSize; // NOLINT

    {
      std::lock_guard<std::mutex> lock{
          Graphics::GraphicsContext::mutexes.device};

      vkAllocateCommandBuffers(context.device, &allocInfo, writeAddress);
    }
  }

  int index = 0;
  std::vector<CmdBufferSubmitInfo> cmdBuffersToSubmit;

  bool transitioned = false;

  for (auto &cmdBuffer : combined) {
    cmdBuffersToSubmit.push_back({
        .buffer = availableCommandBuffers.at(index),
        .queueFamily = static_cast<uint32_t>(cmdBuffer.first),
    });

    if (cmdBuffer.first == context.graphicsQueueFamily) {
      ERR_ASSERT(!transitioned);

      auto &threadData = GetThreadContext();
      threadData.commandBuffer = cmdBuffer.second;

      CHECK_ERR(swapchainManager.EndFrame(context));

      VkMemoryBarrier2 barrier{
          .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
          .pNext = nullptr,
          .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .srcAccessMask =
              VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .dstAccessMask =
              VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
      };

      VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                           .memoryBarrierCount = 1,
                           .pMemoryBarriers = &barrier};

      CHECK_ERR(cmdBuffer.second->PipelineBarrier2({&dep}));

      threadData.commandBuffer = nullptr;

      transitioned = true;
    }

    CHECK_ERR(graph.Submit(context, *cmdBuffer.second));
    CHECK_ERR(graph.Write(context, availableCommandBuffers.at(index)));
    graph = {};

    index++;
  }

  ERR_ASSERT(transitioned);

  CHECK_ERR(SubmitCommandBuffers(context, cmdBuffersToSubmit));

  CHECK_ERR(PresentFrame(context));

  context.currentFrame++;
  context.frameIndex = context.currentFrame % FRAMES_IN_FLIGHT;

  auto *windowContext = Window::GetWindowContext();
  CHECK_NULL(windowContext);

  // TODO: Improve this
  if (windowContext->swapchainOutOfDate) {
    windowContext->swapchainOutOfDate = false;
    swapchainManager.MakeDirty();
  }

  CHECK_ERR(
      swapchainManager.NewFrame(context, *windowContext, context.currentFrame));

  CHECK_ERR(AcquireNextSwapchainImage(context));

  CHECK_ERR(PrepareRecording(context));

  Graphics::SetDirtyState();
  CHECK_ERR(RenderState::BeginFrame(context));

  GetGlobalUniformBuffer(context.frameIndex).NewFrame();

  context.currentlyReordering = false;

  return Error::Success();
}
} // namespace Graphics