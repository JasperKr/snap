#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/resource.hpp"
#include "Graphics/semaphoreManager.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

struct CompactionEvent {
  VkDeviceSize compactedSize;
  struct BLAS *blas;
};

struct BVHManager {
public:
  static const size_t InitialScratchBufferSize = 1UL * 1024 * 1024;
  static std::mutex CompactionMutex;
  static std::mutex ScratchBufferMutex;

  auto GetBVHScratchBuffer(const GraphicsContext &context,
                           VkDeviceSize minimumSize) -> Result<Ref<Buffer>>;

  auto Initialize(const GraphicsContext &context) -> Error;
  auto DeInitialize() -> void;
  auto Update(const GraphicsContext &context) -> Error;

  [[nodiscard]] auto GetCompactionEvents() -> std::vector<CompactionEvent> & {
    return CompactionEvents;
  }

  auto AddCompactionEvent(const CompactionEvent &event) -> void {
    std::lock_guard<std::mutex> lock(CompactionMutex);

    CompactionEvents.emplace_back(event);
  }

private:
  Ref<Buffer> BvhScratchBuffer;

  std::vector<CompactionEvent> CompactionEvents;
};

extern BVHManager BVHManagerInstance; // NOLINT

auto InitializeBVHModule(const struct GraphicsContext &context) -> Error;
auto DeInitializeBVHModule() -> void;

static const Type LuaBLASType = Type("BottomLevelAccelerationStructure");
static const Type LuaTLASType = Type("TopLevelAccelerationStructure");
static constexpr size_t InitialTLASInstanceBufferCapacity =
    1024UL; // 1k instances

// Bottom-Level Acceleration Structure.
struct BLAS : Object, Identifiable {
  BLAS() = default;

  BLAS(const BLAS &) = delete;
  BLAS(BLAS &&) = delete;
  auto operator=(const BLAS &) -> BLAS & = delete;
  auto operator=(BLAS &&) -> BLAS & = delete;

  ~BLAS() override {
    if (accelerationStructureBuffer != nullptr) {
      TotalAllocatedMemory.fetch_sub(accelerationStructureBuffer->size);
    }

    ScheduleDestruction(
        AccelerationStructureMemory{
            .accelerationStructure = accelerationStructure,
        },
        SemaphoreManager::GetSemaphoreValue());
  }

  static auto Create(const GraphicsContext &context, const struct Mesh &mesh)
      -> Result<Ref<BLAS>>;

  static auto GetType() -> Type const * { return &LuaBLASType; }
  auto GetInstanceType() const -> Type const * override {
    return BLAS::GetType();
  }

  [[nodiscard]] auto GetDeviceAddress() const -> VkDeviceAddress;

  auto Rebuild(const GraphicsContext &context) -> Error;
  auto Refit(const GraphicsContext &context) -> Error;
  auto Compact(const GraphicsContext &context) -> Error;

  // DO NOT USE. INTERNAL
  auto FinalizeCompaction(const GraphicsContext &context,
                          VkDeviceSize compactedSize) -> Error;

  static std::atomic<size_t> TotalAllocatedMemory;

private:
  Ref<Buffer> accelerationStructureBuffer;
  VkDeviceAddress accelerationStructureAddress = 0;
  VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;

  Ref<Buffer> vertexBuffer;
  Ref<Buffer> indexBuffer;
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
  VkIndexType indexFormat = VK_INDEX_TYPE_NONE_KHR;
  VkFormat vertexFormat = VK_FORMAT_UNDEFINED;
  uint32_t vertexStride = 0;
  uint32_t vertexOffset = 0;

  std::mutex mutex;
};

// Top-Level Acceleration Structure.
struct TLAS : Object, Identifiable {
  TLAS() = default;

  TLAS(const TLAS &) = delete;
  TLAS(TLAS &&) = delete;
  auto operator=(const TLAS &) -> TLAS & = delete;
  auto operator=(TLAS &&) -> TLAS & = delete;

  ~TLAS() override {
    if (accelerationStructureBuffer != nullptr) {
      TotalAllocatedMemory.fetch_sub(accelerationStructureBuffer->size);
    }

    ScheduleDestruction(
        AccelerationStructureMemory{
            .accelerationStructure = accelerationStructure,
        },
        SemaphoreManager::GetSemaphoreValue());
  }

  static auto Create(const GraphicsContext &context,
                     const std::string_view &debugname = "Unnamed TLAS")
      -> Result<Ref<TLAS>>;

  static auto GetType() -> Type const * { return &LuaTLASType; }
  auto GetInstanceType() const -> Type const * override {
    return TLAS::GetType();
  }

  [[nodiscard]] auto GetDeviceAddress() const -> VkDeviceAddress;

  auto AddInstance(const Ref<BLAS> &blas, const Math::Matrix4x4 &transform)
      -> Result<uint32_t>;
  auto RemoveInstance(uint32_t index) -> void;
  auto UpdateInstance(uint32_t index, const Math::Matrix4x4 &transform) -> void;

  auto GetInstances() const
      -> const std::vector<VkAccelerationStructureInstanceKHR> & {
    return instances;
  }

  auto GetInstanceBuffer() const -> Ref<Buffer> { return instanceBuffer; }
  auto GetTLASBuffer() const -> Ref<Buffer> {
    return accelerationStructureBuffer;
  }

  auto GetAccelerationStructure() const -> VkAccelerationStructureKHR {
    return accelerationStructure;
  }

  auto Refit(const GraphicsContext &context) -> Error;
  auto Rebuild(const GraphicsContext &context) -> Error;

  auto GetDebugName() const -> std::string_view { return debugName; }

  auto MarkUse() -> void {
    if (instanceBuffer != nullptr) {
      instanceBuffer->MarkUse();
    }

    if (accelerationStructureBuffer != nullptr) {
      accelerationStructureBuffer->MarkUse();
    }
  }

  static std::atomic<size_t> TotalAllocatedMemory;

private:
  Ref<Buffer> accelerationStructureBuffer;
  VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
  VkDeviceAddress accelerationStructureAddress = 0;

  std::vector<VkAccelerationStructureInstanceKHR> instances;

  Ref<Buffer> instanceBuffer;
  uint32_t instanceCount = 0;

  size_t instanceCapacity = InitialTLASInstanceBufferCapacity;
  std::string debugName = "Unnamed TLAS";
};

} // namespace Graphics