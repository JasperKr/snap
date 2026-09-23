#pragma once

#include <cstddef>
#include <cstdint>
namespace Hash {

struct Hasher {
  Hasher() = default;

  auto Add(size_t value) -> void;
  auto Add(uint32_t value) -> void;
  auto Add(int32_t value) -> void;
  auto Add(float value) -> void;
  auto Add(double value) -> void;
  auto Add(void const *ptr) -> void;
  auto Add(const char *str) -> void;

  [[nodiscard]] auto Get() const -> size_t;
  auto Reset() -> void;

private:
  size_t hashedValue = 0;
};

} // namespace Hash