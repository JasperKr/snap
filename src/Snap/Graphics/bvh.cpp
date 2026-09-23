#include "bvh.hpp"
#include "Graphics/FrameGraph/commands.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics {

std::atomic<size_t> TLAS::TotalAllocatedMemory;
std::atomic<size_t> BLAS::TotalAllocatedMemory;

std::mutex BVHManager::ScratchBufferMutex;
std::mutex BVHManager::CompactionMutex;

BVHManager BVHManagerInstance; // NOLINT

auto BVHManager::GetBVHScratchBuffer(const GraphicsContext &context,
                                     VkDeviceSize minimumSize)
    -> Result<Ref<Buffer>> {
  auto size = BvhScratchBuffer.isValid() ? BvhScratchBuffer->size : 0UL;
  auto newSize = Utils::NextCapacity(size, minimumSize,
                                     BVHManager::InitialScratchBufferSize);

  if (newSize != size) {
    auto info = BufferCreationInfo{
        .size = newSize,
        .usage = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .stagingBuffer = false,
        .persistentMapping = false,
        .debugName = "BVH Scratch Buffer",
    };

    BvhScratchBuffer = CHECK_RES(Buffer::Create(context, info));
  }

  return BvhScratchBuffer;
}

auto BVHManager::Initialize(const GraphicsContext &context) -> Error {
  auto info = BufferCreationInfo{
      .size = BVHManager::InitialScratchBufferSize,
      .usage = static_cast<uint32_t>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) |
               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      .stagingBuffer = false,
      .persistentMapping = false,
      .debugName = "BVH Scratch Buffer",
  };

  BvhScratchBuffer = CHECK_RES(Buffer::Create(context, info));

  return {};
}

auto BVHManager::DeInitialize() -> void {
  BvhScratchBuffer = nullptr;

  std::lock_guard<std::mutex> lock(CompactionMutex);
  CompactionEvents.clear();
}

auto BVHManager::Update(const GraphicsContext &context) -> Error {
  std::lock_guard<std::mutex> lock(CompactionMutex);

  for (const auto &event : CompactionEvents) {
    auto compactedSize = event.compactedSize;

    CHECK_ERR(event.blas->FinalizeCompaction(context, compactedSize));
  }

  CompactionEvents.clear();

  return {};
}

auto InitializeBVHModule(const GraphicsContext &context) -> Error {
  BVHManagerInstance = BVHManager();
  CHECK_ERR(BVHManagerInstance.Initialize(context));
  return {};
}

auto DeInitializeBVHModule() -> void { BVHManagerInstance.DeInitialize(); }

auto BLAS::Create(const GraphicsContext &context, const Mesh &mesh)
    -> Result<Ref<BLAS>> {
  std::lock_guard<std::mutex> lock(BVHManager::ScratchBufferMutex);

  const auto *positionAttribute =
      mesh.GetVertexFormat().GetAttribute("POSITION");
  ERR_ASSERT_MSG(positionAttribute != nullptr,
                 "BLAS build requires a 'POSITION' vertex attribute");

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR |
                   VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

  auto positionAddress = CHECK_RES(
      mesh.GetVertexBuffer(positionAttribute->binding)->GetDeviceAddress());
  positionAddress += positionAttribute->offset;

  if (mesh.GetIndexBuffer() != nullptr && mesh.GetIndexCount() > 0) {
    auto indexAddress = CHECK_RES(mesh.GetIndexBuffer()->GetDeviceAddress());

    geometry.geometry.triangles = {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = positionAttribute->format,
        .vertexData =
            {
                .deviceAddress = positionAddress,
            },
        .vertexStride =
            mesh.GetVertexFormat().GetStride(positionAttribute->binding),
        .maxVertex = mesh.GetVertexCount() - 1,
        .indexType = mesh.GetIndexFormat(),
        .indexData =
            {
                .deviceAddress = indexAddress,
            },
    };
  } else {
    geometry.geometry.triangles = {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = positionAttribute->format,
        .vertexData =
            {
                .deviceAddress = positionAddress,
            },
        .vertexStride =
            mesh.GetVertexFormat().GetStride(positionAttribute->binding),
        .maxVertex = mesh.GetVertexCount() - 1,
        .indexType = VK_INDEX_TYPE_NONE_KHR,
    };
  }

  uint32_t primitiveCount = mesh.GetIndexCount() / 3;
  if (mesh.GetIndexCount() == 0) {
    primitiveCount = mesh.GetVertexCount() / 3;
  }

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  // Query required sizes
  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &primitiveCount, &sizeInfo);

  auto bvh = Ref<BLAS>::Make();
  bvh->vertexBuffer = mesh.GetVertexBuffer(positionAttribute->binding);
  bvh->indexBuffer = mesh.GetIndexBuffer();
  bvh->vertexCount = mesh.GetVertexCount();
  bvh->indexCount = mesh.GetIndexCount();
  bvh->indexFormat = mesh.GetIndexFormat();
  bvh->vertexFormat = positionAttribute->format;
  bvh->vertexStride =
      mesh.GetVertexFormat().GetStride(positionAttribute->binding);
  bvh->vertexOffset = positionAttribute->offset;

  // Create AS storage buffer
  bvh->accelerationStructureBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = sizeInfo.accelerationStructureSize,
          .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = false,
          .persistentMapping = false,
          .debugName = "BLAS Buffer",
      }));
  BLAS::TotalAllocatedMemory.fetch_add(sizeInfo.accelerationStructureSize);

  // Create VkAccelerationStructureKHR
  VkAccelerationStructureCreateInfoKHR asCreateInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = bvh->accelerationStructureBuffer->handle,
      .size = sizeInfo.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
        context.device, &asCreateInfo, GetAllocationCallbacks(),
        &bvh->accelerationStructure));
  }

  auto alignment = context.accelerationStructureProperties
                       .minAccelerationStructureScratchOffsetAlignment;

  auto scratchBuffer = CHECK_RES(BVHManagerInstance.GetBVHScratchBuffer(
      context, sizeInfo.buildScratchSize + alignment));
  auto scratchAddress = CHECK_RES(scratchBuffer->GetDeviceAddress());
  scratchAddress = Utils::AlignUp(scratchAddress, alignment);

  // Finalize build info
  buildInfo.dstAccelerationStructure = bvh->accelerationStructure;
  buildInfo.scratchData.deviceAddress = scratchAddress;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = primitiveCount,
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  const VkAccelerationStructureBuildRangeInfoKHR *rangePtr = &range;

  auto *cmdBuffer = GetVirtualCommandBuffer();
  ERR_ASSERT(cmdBuffer != nullptr);

  scratchBuffer->MarkUse();

  std::vector<VkBuffer> reads{mesh.GetVertexBuffer()->handle,
                              scratchBuffer->handle};
  std::vector<VkBuffer> writes{bvh->accelerationStructureBuffer->handle,
                               scratchBuffer->handle};

  if (mesh.GetIndexBuffer() != nullptr) {
    reads.emplace_back(mesh.GetIndexBuffer()->handle);
  }

  CHECK_ERR(cmdBuffer->BuildAccelerationStructuresKHR(
      {1, &buildInfo, &rangePtr, reads, writes}));

  bvh->accelerationStructureBuffer->MarkUse();

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = bvh->accelerationStructure,
  };

  bvh->accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  return bvh;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto BLAS::Rebuild(const GraphicsContext &context) -> Error {
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(accelerationStructureBuffer != nullptr);
  ERR_ASSERT(vertexBuffer != nullptr);

  std::lock_guard<std::mutex> lock(BVHManager::ScratchBufferMutex);
  std::unique_lock<std::mutex> lock2(mutex);

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR |
                   VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

  auto positionAddress = CHECK_RES(vertexBuffer->GetDeviceAddress());
  positionAddress += vertexOffset;

  if (indexBuffer != nullptr && indexCount > 0) {
    auto indexAddress = CHECK_RES(indexBuffer->GetDeviceAddress());
    geometry.geometry.triangles = {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = vertexFormat,
        .vertexData =
            {
                .deviceAddress = positionAddress,
            },
        .vertexStride = vertexStride,
        .maxVertex = vertexCount - 1,
        .indexType = indexFormat,
        .indexData =
            {
                .deviceAddress = indexAddress,
            },
    };
  } else {
    geometry.geometry.triangles = {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = vertexFormat,
        .vertexData =
            {
                .deviceAddress = positionAddress,
            },
        .vertexStride = vertexStride,
        .maxVertex = vertexCount - 1,
        .indexType = VK_INDEX_TYPE_NONE_KHR,
    };
  }

  uint32_t primitiveCount = indexCount / 3;
  if (indexCount == 0) {
    primitiveCount = vertexCount / 3;
  }

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &primitiveCount, &sizeInfo);

  auto alignment = context.accelerationStructureProperties
                       .minAccelerationStructureScratchOffsetAlignment;

  auto scratchBuffer = CHECK_RES(BVHManagerInstance.GetBVHScratchBuffer(
      context, sizeInfo.updateScratchSize + alignment));
  auto scratchAddress = CHECK_RES(scratchBuffer->GetDeviceAddress());
  scratchAddress = Utils::AlignUp(scratchAddress, alignment);

  buildInfo.scratchData.deviceAddress = scratchAddress;

  if (accelerationStructureBuffer->size < sizeInfo.accelerationStructureSize) {
    auto oldSize = accelerationStructureBuffer->size;

    ScheduleDestruction(
        AccelerationStructureMemory{
            .accelerationStructure = accelerationStructure,
        },
        SemaphoreManager::GetSemaphoreValue());

    accelerationStructureBuffer = CHECK_RES(Buffer::Create(
        context,
        {
            .size = sizeInfo.accelerationStructureSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .stagingBuffer = false,
            .persistentMapping = false,
            .debugName = "BLAS Buffer",
        }));
    BLAS::TotalAllocatedMemory.fetch_sub(oldSize);
    BLAS::TotalAllocatedMemory.fetch_add(sizeInfo.accelerationStructureSize);

    VkAccelerationStructureCreateInfoKHR asCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = accelerationStructureBuffer->handle,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    };

    {
      std::lock_guard<std::mutex> lockDevice(
          Graphics::GraphicsContext::mutexes.device);

      CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
          context.device, &asCreateInfo, GetAllocationCallbacks(),
          &accelerationStructure));
    }
  }

  buildInfo.dstAccelerationStructure = accelerationStructure;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = primitiveCount,
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  const auto *rangePtr = &range;

  auto *cmdBuffer = GetVirtualCommandBuffer();
  ERR_ASSERT(cmdBuffer != nullptr);

  scratchBuffer->MarkUse();
  vertexBuffer->MarkUse();
  if (indexBuffer != nullptr && indexCount > 0) {
    indexBuffer->MarkUse();
  }

  std::vector<VkBuffer> reads{vertexBuffer->handle, scratchBuffer->handle};
  if (indexBuffer != nullptr && indexCount > 0) {
    reads.push_back(indexBuffer->handle);
  }
  std::vector<VkBuffer> writes{accelerationStructureBuffer->handle,
                               scratchBuffer->handle};

  // vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);
  CHECK_ERR(cmdBuffer->BuildAccelerationStructuresKHR(
      {1, &buildInfo, &rangePtr, reads, writes}));

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = accelerationStructure,
  };

  accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  return Error::Success();
}

auto BLAS::Refit(const GraphicsContext &context) -> Error {
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(accelerationStructureBuffer != nullptr);
  ERR_ASSERT(vertexBuffer != nullptr);

  std::lock_guard<std::mutex> lock(BVHManager::ScratchBufferMutex);
  std::unique_lock<std::mutex> lock2(mutex);

  VkAccelerationStructureGeometryKHR geometry{};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR |
                   VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

  auto positionAddress = CHECK_RES(vertexBuffer->GetDeviceAddress());
  positionAddress += vertexOffset;

  if (indexBuffer != nullptr && indexCount > 0) {
    auto indexAddress = CHECK_RES(indexBuffer->GetDeviceAddress());
    geometry.geometry.triangles = {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = vertexFormat,
        .vertexData =
            {
                .deviceAddress = positionAddress,
            },
        .vertexStride = vertexStride,
        .maxVertex = vertexCount - 1,
        .indexType = indexFormat,
        .indexData =
            {
                .deviceAddress = indexAddress,
            },
    };
  } else {
    geometry.geometry.triangles = {
        .sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = vertexFormat,
        .vertexData =
            {
                .deviceAddress = positionAddress,
            },
        .vertexStride = vertexStride,
        .maxVertex = vertexCount - 1,
        .indexType = VK_INDEX_TYPE_NONE_KHR,
    };
  }

  uint32_t primitiveCount = indexCount / 3;
  if (indexCount == 0) {
    primitiveCount = vertexCount / 3;
  }

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      .flags = static_cast<uint32_t>(
                   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
      .srcAccelerationStructure = accelerationStructure,
      .dstAccelerationStructure = accelerationStructure,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = primitiveCount,
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  const auto *rangePtr = &range;

  auto *cmdBuffer = GetVirtualCommandBuffer();
  ERR_ASSERT(cmdBuffer != nullptr);

  vertexBuffer->MarkUse();
  if (indexBuffer != nullptr && indexCount > 0) {
    indexBuffer->MarkUse();
  }

  std::vector<VkBuffer> reads{vertexBuffer->handle};
  if (indexBuffer != nullptr && indexCount > 0) {
    reads.push_back(indexBuffer->handle);
  }
  std::vector<VkBuffer> writes{accelerationStructureBuffer->handle};

  // vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);
  CHECK_ERR(cmdBuffer->BuildAccelerationStructuresKHR(
      {1, &buildInfo, &rangePtr, reads, writes}));

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = accelerationStructure,
  };

  accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  return Error::Success();
}

auto BLAS::Compact(const GraphicsContext &context) -> Error {
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(accelerationStructureBuffer != nullptr);

  VkQueryPool queryPool = VK_NULL_HANDLE;

  VkQueryPoolCreateInfo queryPoolCreateInfo{
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
      .queryCount = 1,
  };

  {
    std::lock_guard<std::mutex> lockDevice(
        Graphics::GraphicsContext::mutexes.device);
    CHECK_NEW_ERR(vkCreateQueryPool(context.device, &queryPoolCreateInfo,
                                    GetAllocationCallbacks(), &queryPool));
  }

  auto *commandbuffer = CHECK_NULL(GetVirtualCommandBuffer());

  CHECK_ERR(commandbuffer->ResetQueryPool({queryPool, 0, 1}));
  CHECK_ERR(commandbuffer->WriteAccelerationStructuresPropertiesKHR(
      {1, &accelerationStructure,
       VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0}));

  uint64_t queryTimelineValue = Graphics::SemaphoreManager::GetSemaphoreValue();

  // Lambda that wraps a function and prints any errors it returns
  auto errorHandler =
      [](const std::function<Error()> &func) -> std::function<void()> {
    return [func]() -> void {
      auto err = func();
      if (Error::IsError(err)) {
        PrintError(err.ToString());
      }
    };
  };

  PrintAlways("Compacting BLAS (current size: {} bytes)",
              accelerationStructureBuffer->size);

  auto readbackThread = std::thread(errorHandler([context, queryPool,
                                                  queryTimelineValue,
                                                  this]() -> Error {
    std::unique_lock<std::mutex> lock(mutex);

    {
      std::unique_lock<std::mutex> lock(
          Graphics::semaphoreManager.timelineCompletionMutex);
      Graphics::semaphoreManager.timelineCompletionCV.wait(lock, [&]() -> bool {
        return !Graphics::semaphoreManager.IsInUse(queryTimelineValue);
      });
    }

    // The query has completed, we can now read back the compacted size

    VkDeviceSize compactedSize = 0;
    {
      CHECK_NEW_ERR(vkGetQueryPoolResults(
          context.device, queryPool, 0, 1, sizeof(compactedSize),
          &compactedSize, sizeof(compactedSize),
          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

      std::lock_guard<std::mutex> lockDevice(
          Graphics::GraphicsContext::mutexes.device);
      vkDestroyQueryPool(context.device, queryPool, GetAllocationCallbacks());
    }

    if (compactedSize == 0 ||
        accelerationStructureBuffer->size <= compactedSize) {
      return Error::Success();
    }

    BVHManagerInstance.AddCompactionEvent(
        {.compactedSize = compactedSize, .blas = this});

    return {};
  }));

  readbackThread.detach();
  accelerationStructureBuffer->MarkUse();

  return Error::Success();
}

auto BLAS::FinalizeCompaction(const GraphicsContext &context,
                              VkDeviceSize compactedSize) -> Error {
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(accelerationStructureBuffer != nullptr);

  std::lock_guard<std::mutex> lock(BVHManager::ScratchBufferMutex);
  std::lock_guard<std::mutex> lock2(mutex);

  auto compactedBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = compactedSize,
          .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = false,
          .persistentMapping = false,
          .debugName = "BLAS Buffer (Compacted)",
      }));

  VkAccelerationStructureKHR compactedAccelerationStructure = VK_NULL_HANDLE;

  VkAccelerationStructureCreateInfoKHR asCreateInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = compactedBuffer->handle,
      .size = compactedSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
  };

  {
    std::lock_guard<std::mutex> lockDevice(
        Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
        context.device, &asCreateInfo, GetAllocationCallbacks(),
        &compactedAccelerationStructure));
  }

  VkCopyAccelerationStructureInfoKHR copyInfo{
      .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
      .src = accelerationStructure,
      .dst = compactedAccelerationStructure,
      .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR,
  };

  auto *commandbuffer = CHECK_NULL(GetVirtualCommandBuffer());

  CHECK_ERR(commandbuffer->CopyAccelerationStructureKHR({&copyInfo}));

  ScheduleDestruction(
      AccelerationStructureMemory{
          .accelerationStructure = accelerationStructure,
      },
      SemaphoreManager::GetSemaphoreValue());

  BLAS::TotalAllocatedMemory.fetch_sub(accelerationStructureBuffer->size);
  BLAS::TotalAllocatedMemory.fetch_add(compactedSize);

  accelerationStructure = compactedAccelerationStructure;
  accelerationStructureBuffer->MarkUse();

  accelerationStructureBuffer = compactedBuffer;
  accelerationStructureBuffer->MarkUse();

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = accelerationStructure,
  };

  {
    std::lock_guard<std::mutex> lockDevice(
        Graphics::GraphicsContext::mutexes.device);

    accelerationStructureAddress = vkGetAccelerationStructureDeviceAddressKHR(
        context.device, &addressInfo);
  }

  return Error::Success();
}

auto BLAS::GetDeviceAddress() const -> VkDeviceAddress {
  return accelerationStructureAddress;
}

auto TLAS::GetDeviceAddress() const -> VkDeviceAddress {
  return accelerationStructureAddress;
}

auto TLAS::Create(const GraphicsContext &context,
                  const std::string_view &debugname) -> Result<Ref<TLAS>> {
  std::lock_guard<std::mutex> lock(BVHManager::ScratchBufferMutex);

  auto tlas = Ref<TLAS>::Make();

  tlas->debugName = std::string(debugname);

  // Filled when adding instances
  std::vector<VkAccelerationStructureInstanceKHR> instances;

  // Upload instances to GPU
  tlas->instanceBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = sizeof(VkAccelerationStructureInstanceKHR) *
                  tlas->instanceCapacity,
          .usage =
              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = true,
          .persistentMapping = false,
          .debugName = "TLAS Instances",
      }));

  auto instanceAddress = CHECK_RES(tlas->instanceBuffer->GetDeviceAddress());

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
  };

  geometry.geometry.instances = {
      .sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
      .arrayOfPointers = VK_FALSE,
      .data =
          {
              .deviceAddress = instanceAddress,
          },
  };

  auto instanceCount = tlas->instanceCount;
  auto maxInstanceCount = static_cast<uint32_t>(tlas->instanceCapacity);

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &maxInstanceCount, &sizeInfo);

  // Create AS storage buffer
  tlas->accelerationStructureBuffer = CHECK_RES(Buffer::Create(
      context,
      {
          .size = sizeInfo.accelerationStructureSize,
          .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
          .stagingBuffer = false,
          .persistentMapping = false,
          .debugName = "TLAS Buffer",
      }));
  TLAS::TotalAllocatedMemory.fetch_add(sizeInfo.accelerationStructureSize);

  // Create VkAccelerationStructureKHR
  VkAccelerationStructureCreateInfoKHR asCreateInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = tlas->accelerationStructureBuffer->handle,
      .size = sizeInfo.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
  };

  {
    std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);

    CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
        context.device, &asCreateInfo, GetAllocationCallbacks(),
        &tlas->accelerationStructure));
  }

  auto alignment = context.accelerationStructureProperties
                       .minAccelerationStructureScratchOffsetAlignment;

  auto scratchBuffer = CHECK_RES(BVHManagerInstance.GetBVHScratchBuffer(
      context, sizeInfo.buildScratchSize + alignment));
  auto scratchAddress = CHECK_RES(scratchBuffer->GetDeviceAddress());
  scratchAddress = Utils::AlignUp(scratchAddress, alignment);

  // Finalize build info

  buildInfo.dstAccelerationStructure = tlas->accelerationStructure;
  buildInfo.scratchData.deviceAddress = scratchAddress;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = instanceCount,
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };

  const VkAccelerationStructureBuildRangeInfoKHR *rangePtr = &range;

  auto *cmdBuffer = GetVirtualCommandBuffer();
  ERR_ASSERT(cmdBuffer != nullptr);

  scratchBuffer->MarkUse();

  std::vector<VkBuffer> reads{tlas->instanceBuffer->handle,
                              scratchBuffer->handle};
  std::vector<VkBuffer> writes{tlas->accelerationStructureBuffer->handle,
                               scratchBuffer->handle};

  // vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);
  CHECK_ERR(cmdBuffer->BuildAccelerationStructuresKHR(
      {1, &buildInfo, &rangePtr, reads, writes}));

  tlas->accelerationStructureBuffer->MarkUse();

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = tlas->accelerationStructure,
  };

  tlas->accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  tlas->instanceCount = static_cast<uint32_t>(tlas->instances.size());

  return tlas;
}

auto TLAS::AddInstance(const Ref<BLAS> &blas, const Math::Matrix4x4 &transform)
    -> Result<uint32_t> {
  ERR_ASSERT(blas != nullptr);

  VkAccelerationStructureInstanceKHR instance{};

  // clang-format off
  instance.transform = VkTransformMatrixKHR{
      transform.At(0, 0), transform.At(1, 0), transform.At(2, 0), transform.At(3, 0),
      transform.At(0, 1), transform.At(1, 1), transform.At(2, 1), transform.At(3, 1),
      transform.At(0, 2), transform.At(1, 2), transform.At(2, 2), transform.At(3, 2),
  };
  // clang-format on

  instance.instanceCustomIndex = static_cast<uint32_t>(instances.size());
  instance.mask = 0xFF; // All bits enabled NOLINT
  instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  instance.accelerationStructureReference = blas->GetDeviceAddress();

  instances.emplace_back(instance);

  return static_cast<uint32_t>(instances.size() - 1);
}

auto TLAS::RemoveInstance(uint32_t index) -> void {
  if (index >= instances.size()) {
    return;
  }

  instances.erase(instances.begin() + index);
}

auto TLAS::UpdateInstance(uint32_t index, const Math::Matrix4x4 &transform)
    -> void {
  if (index >= instances.size()) {
    return;
  }

  VkAccelerationStructureInstanceKHR &instance = instances.at(index);

  // clang-format off
  instance.transform = VkTransformMatrixKHR{
      transform.At(0, 0), transform.At(1, 0), transform.At(2, 0), transform.At(3, 0),
      transform.At(0, 1), transform.At(1, 1), transform.At(2, 1), transform.At(3, 1),
      transform.At(0, 2), transform.At(1, 2), transform.At(2, 2), transform.At(3, 2),
  };
  // clang-format on
}

auto TLAS::Refit(const GraphicsContext &context) -> Error {
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(instanceBuffer != nullptr);
  ERR_ASSERT(instances.size() > 0);

  [[unlikely]]
  if (instances.size() != instanceCount) {
    PrintWarning(
        "Refit called on TLAS with a different number of instances than "
        "originally built. Refit is only valid if the number of instances "
        "remains the same.");
  }

  auto instanceAddress = CHECK_RES(instanceBuffer->GetDeviceAddress());

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry = {
          .instances = {
              .sType =
                  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
              .arrayOfPointers = VK_FALSE,
              .data = {.deviceAddress = instanceAddress},
          }}};

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
      .srcAccelerationStructure = accelerationStructure,
      .dstAccelerationStructure = accelerationStructure,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  auto primitiveCount = static_cast<uint32_t>(instances.size());
  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &primitiveCount, &sizeInfo);

  auto alignment = context.accelerationStructureProperties
                       .minAccelerationStructureScratchOffsetAlignment;

  auto scratchBuffer = CHECK_RES(BVHManagerInstance.GetBVHScratchBuffer(
      context, sizeInfo.updateScratchSize + alignment));
  auto scratchAddress = CHECK_RES(scratchBuffer->GetDeviceAddress());
  scratchAddress = Utils::AlignUp(scratchAddress, alignment);

  buildInfo.scratchData.deviceAddress = scratchAddress;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = static_cast<uint32_t>(instances.size()),
  };

  const auto *rangePtr = &range;

  CHECK_ERR(instanceBuffer->SetData(
      context, std::span<const VkAccelerationStructureInstanceKHR>(
                   instances.data(), instances.size())));

  scratchBuffer->MarkUse();
  instanceBuffer->MarkUse();

  std::vector<VkBuffer> reads{instanceBuffer->handle, scratchBuffer->handle};
  std::vector<VkBuffer> writes{accelerationStructureBuffer->handle,
                               scratchBuffer->handle};

  // vkCmdBuildAccelerationStructuresKHR(GetCommandBuffer(), 1, &buildInfo,
  //                                     &rangePtr);
  CHECK_ERR(GetVirtualCommandBuffer()->BuildAccelerationStructuresKHR(
      {1, &buildInfo, &rangePtr, reads, writes}));

  instanceCount = static_cast<uint32_t>(instances.size());

  return Error::Success();
}

auto TLAS::Rebuild(const GraphicsContext &context) -> Error {
  ERR_ASSERT(accelerationStructure != VK_NULL_HANDLE);
  ERR_ASSERT(instanceBuffer != nullptr);
  ERR_ASSERT(instances.size() > 0);

  auto newSize = Utils::NextCapacity(instanceCapacity, instances.size());
  if (newSize != instanceCapacity) {
    instanceBuffer = CHECK_RES(Buffer::Create(
        context,
        {
            .size = sizeof(VkAccelerationStructureInstanceKHR) * newSize,
            .usage =
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .stagingBuffer = true,
            .persistentMapping = false,
            .debugName = "TLAS Instances",
        }));

    instanceCapacity = newSize;
  }

  auto instanceAddress = CHECK_RES(instanceBuffer->GetDeviceAddress());

  VkAccelerationStructureGeometryKHR geometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry = {
          .instances = {
              .sType =
                  VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
              .arrayOfPointers = VK_FALSE,
              .data = {
                  .deviceAddress = instanceAddress,
              }}}};

  VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
      .dstAccelerationStructure = accelerationStructure,
      .geometryCount = 1,
      .pGeometries = &geometry,
  };

  auto primitiveCount = static_cast<uint32_t>(instances.size());
  VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };

  vkGetAccelerationStructureBuildSizesKHR(
      context.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &buildInfo, &primitiveCount, &sizeInfo);

  if (accelerationStructureBuffer->size < sizeInfo.accelerationStructureSize) {
    auto oldSize = accelerationStructureBuffer->size;

    ScheduleDestruction(
        AccelerationStructureMemory{
            .accelerationStructure = accelerationStructure,
        },
        SemaphoreManager::GetSemaphoreValue());

    accelerationStructureBuffer->MarkUse();

    accelerationStructureBuffer = CHECK_RES(Buffer::Create(
        context,
        {
            .size = sizeInfo.accelerationStructureSize,
            .usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .stagingBuffer = false,
            .persistentMapping = false,
            .debugName = "TLAS Buffer",
        }));
    TLAS::TotalAllocatedMemory.fetch_sub(oldSize);
    TLAS::TotalAllocatedMemory.fetch_add(sizeInfo.accelerationStructureSize);

    VkAccelerationStructureCreateInfoKHR asCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = accelerationStructureBuffer->handle,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    };

    {
      std::lock_guard<std::mutex> lock(
          Graphics::GraphicsContext::mutexes.device);

      CHECK_NEW_ERR(vkCreateAccelerationStructureKHR(
          context.device, &asCreateInfo, GetAllocationCallbacks(),
          &accelerationStructure));
    }
  }

  auto alignment = context.accelerationStructureProperties
                       .minAccelerationStructureScratchOffsetAlignment;

  auto scratchBuffer = CHECK_RES(BVHManagerInstance.GetBVHScratchBuffer(
      context, sizeInfo.buildScratchSize + alignment));
  auto scratchAddress = CHECK_RES(scratchBuffer->GetDeviceAddress());
  scratchAddress = Utils::AlignUp(scratchAddress, alignment);

  buildInfo.scratchData.deviceAddress = scratchAddress;

  buildInfo.dstAccelerationStructure = accelerationStructure;

  VkAccelerationStructureBuildRangeInfoKHR range{
      .primitiveCount = primitiveCount,
  };

  const auto *rangePtr = &range;

  CHECK_ERR(instanceBuffer->SetData(
      context, std::span<const VkAccelerationStructureInstanceKHR>(
                   instances.data(), instances.size())));

  scratchBuffer->MarkUse();
  instanceBuffer->MarkUse();

  std::vector<VkBuffer> reads{instanceBuffer->handle, scratchBuffer->handle};
  std::vector<VkBuffer> writes{accelerationStructureBuffer->handle,
                               scratchBuffer->handle};

  // vkCmdBuildAccelerationStructuresKHR(GetCommandBuffer(), 1, &buildInfo,
  //                                     &rangePtr);
  CHECK_ERR(GetVirtualCommandBuffer()->BuildAccelerationStructuresKHR(
      {1, &buildInfo, &rangePtr, reads, writes}));

  VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .accelerationStructure = accelerationStructure,
  };

  accelerationStructureAddress =
      vkGetAccelerationStructureDeviceAddressKHR(context.device, &addressInfo);

  instanceCount = static_cast<uint32_t>(instances.size());

  return Error::Success();
}

} // namespace Graphics