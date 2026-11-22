#pragma once

#include "Modules/error.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Graphics {

struct AllocatorInterface {
  virtual ~AllocatorInterface();

  virtual auto Allocate(VkDeviceSize size) -> Result<VkDeviceSize>;
  virtual auto Allocate() -> Result<VkDeviceSize>;

  virtual auto DeAllocate(VkDeviceSize offset) -> Error;
  virtual auto Defragment() -> Error;
  virtual auto HasSpaceFor(VkDeviceSize size) -> bool;
  virtual auto IsEmpty() -> bool;
  virtual auto Resize(VkDeviceSize newSize) -> Error;

  AllocatorInterface(const AllocatorInterface &) = default;
  AllocatorInterface(AllocatorInterface &&) = delete;
  auto operator=(const AllocatorInterface &) -> AllocatorInterface & = default;
  auto operator=(AllocatorInterface &&) -> AllocatorInterface & = delete;
  AllocatorInterface(VkDeviceSize size, VkDeviceSize alignment);

  std::function<Error(VkDeviceSize offset, VkDeviceSize newOffset)> MoveMemoryCallback;
  VkDeviceSize backingAllocationSize;
  VkDeviceSize memoryAlignment = 1;
};

struct LinearAllocator : AllocatorInterface {
  VkDeviceSize pointer{};

  LinearAllocator(const LinearAllocator &) = default;
  LinearAllocator(LinearAllocator &&) = delete;
  auto operator=(const LinearAllocator &) -> LinearAllocator & = default;
  auto operator=(LinearAllocator &&) -> LinearAllocator & = delete;
  LinearAllocator(VkDeviceSize size, VkDeviceSize alignment);
  ~LinearAllocator() override = default;

  auto Allocate(VkDeviceSize size) -> Result<VkDeviceSize> override;
  auto Allocate() -> Result<VkDeviceSize> override;
  auto DeAllocate(VkDeviceSize offset) -> Error override;
  auto Defragment() -> Error override;
  auto HasSpaceFor(VkDeviceSize size) -> bool override { return (pointer + size) < backingAllocationSize; }
  auto IsEmpty() -> bool override { return pointer == 0UL; }
  auto Resize(VkDeviceSize newSize) -> Error override;
};

struct StackAllocator : AllocatorInterface {
  VkDeviceSize pointer{};
  std::vector<VkDeviceSize> allocationAddresses;

  StackAllocator(const StackAllocator &) = default;
  StackAllocator(StackAllocator &&) = delete;
  auto operator=(const StackAllocator &) -> StackAllocator & = default;
  auto operator=(StackAllocator &&) -> StackAllocator & = delete;
  StackAllocator(VkDeviceSize size, VkDeviceSize alignment);
  ~StackAllocator() override = default;

  auto Allocate(VkDeviceSize size) -> Result<VkDeviceSize> override;
  auto Allocate() -> Result<VkDeviceSize> override;
  auto DeAllocate(VkDeviceSize offset) -> Error override;
  auto Defragment() -> Error override;
  auto HasSpaceFor(VkDeviceSize size) -> bool override { return (pointer + size) < backingAllocationSize; }
  auto IsEmpty() -> bool override { return allocationAddresses.empty(); }
  auto Resize(VkDeviceSize newSize) -> Error override;
};

struct PoolAllocator : AllocatorInterface {
  VkDeviceSize chunkSize;
  std::vector<bool> chunksFree;
  std::vector<VkDeviceSize> allocationSizes; // chunk idx -> size

  PoolAllocator(const PoolAllocator &) = default;
  PoolAllocator(PoolAllocator &&) = delete;
  auto operator=(const PoolAllocator &) -> PoolAllocator & = default;
  auto operator=(PoolAllocator &&) -> PoolAllocator & = delete;
  PoolAllocator(VkDeviceSize size, VkDeviceSize chunkSize, VkDeviceSize alignment);
  ~PoolAllocator() override = default;

  auto Allocate(VkDeviceSize size) -> Result<VkDeviceSize> override;
  auto Allocate() -> Result<VkDeviceSize> override;
  auto DeAllocate(VkDeviceSize offset) -> Error override;
  auto Defragment() -> Error override;
  auto HasSpaceFor(VkDeviceSize size) -> bool override;
  auto IsEmpty() -> bool override;
  auto Resize(VkDeviceSize newSize) -> Error override;

private:
  // Find a space for an allocation of a specific size, returning the chunk idx, count
  auto FindSpace(VkDeviceSize size) -> std::optional<std::pair<uint32_t, uint32_t>>;
};

struct GapAllocator : AllocatorInterface {
  VkDeviceSize itemSize;

  VkDeviceSize pointer{};
  std::vector<uint32_t> gaps;

  GapAllocator(const GapAllocator &) = default;
  GapAllocator(GapAllocator &&) = delete;
  auto operator=(const GapAllocator &) -> GapAllocator & = default;
  auto operator=(GapAllocator &&) -> GapAllocator & = delete;
  GapAllocator(VkDeviceSize size, VkDeviceSize itemSize);
  ~GapAllocator() override = default;

  auto Allocate(VkDeviceSize size) -> Result<VkDeviceSize> override;
  auto Allocate() -> Result<VkDeviceSize> override;
  auto DeAllocate(VkDeviceSize offset) -> Error override;
  auto Defragment() -> Error override;
  auto HasSpaceFor(VkDeviceSize size) -> bool override;
  auto IsEmpty() -> bool override;
  auto Resize(VkDeviceSize newSize) -> Error override;
};

// struct LinkedListAllocator : AllocatorInterface {};

} // namespace Graphics