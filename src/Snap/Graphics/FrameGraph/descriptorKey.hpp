#pragma once

#include "Modules/Helpers/hasher.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include <cstddef>

namespace Graphics {

struct ResourceBinding {
  ObjectID resource{};
  uint32_t binding{};

  bool isDynamic = false;

  auto operator==(const ResourceBinding &other) const -> bool {
    return binding == other.binding && resource == other.resource &&
           isDynamic == other.isDynamic;
  }
};

struct DescriptorKey {
  Math::StackVector<ResourceBinding, 16> bindings; // sorted by binding NOLINT

  auto operator==(const DescriptorKey &other) const -> bool {
    return bindings == other.bindings;
  }
};

struct DescriptorKeyHash {
  auto operator()(const DescriptorKey &key) const noexcept -> size_t {
    Hash::Hasher hasher{};

    for (const auto &binding : key.bindings) {
      hasher.Add(binding.binding);
      hasher.Add(binding.resource);
    }

    return hasher.Get();
  }
};

} // namespace Graphics