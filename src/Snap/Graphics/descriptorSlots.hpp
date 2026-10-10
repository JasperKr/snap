#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
using namespace std;

namespace Graphics {

struct DescriptorSlot {
  uint32_t set{};
  uint32_t binding{};

  DescriptorSlot() = default;
  explicit DescriptorSlot(uint64_t slot) { SetSlot(slot); }
  explicit DescriptorSlot(uint32_t set, uint32_t binding)
      : set(set), binding(binding) {};

  [[nodiscard]] auto GetSlot() const -> uint64_t {
    return *reinterpret_cast<uint64_t const *>(this); // NOLINT
  }

  auto SetSlot(uint64_t slot) -> void {
    memcpy((void *)this, &slot, sizeof(uint64_t));
  }

  auto operator==(DescriptorSlot other) const noexcept -> bool {
    return GetSlot() == other.GetSlot();
  }

  auto operator!=(DescriptorSlot other) const noexcept -> bool {
    return GetSlot() != other.GetSlot();
  }
};

} // namespace Graphics

namespace std {
template <> struct hash<Graphics::DescriptorSlot> {
  auto operator()(const Graphics::DescriptorSlot slot) const -> size_t {
    return hash<uint64_t>{}(slot.GetSlot());
  }
};

} // namespace std