#include "hasher.hpp"
#include <cstdint>
#include <functional>
#include <string_view>

namespace Hash {

auto Hasher::Add(size_t value) -> void {
  constexpr uint32_t prime = 0x9e3779b9;
  constexpr uint32_t shift = 6;
  constexpr uint32_t shift2 = 2;

  hashedValue ^= value + prime + (value << shift) + (value >> shift2);
}

auto Hasher::Add(void const *ptr) -> void {
  Add(std::hash<void const *>()(ptr));
}
auto Hasher::Add(uint32_t value) -> void { Add(std::hash<uint32_t>()(value)); }
auto Hasher::Add(int32_t value) -> void { Add(std::hash<int32_t>()(value)); }
auto Hasher::Add(float value) -> void { Add(std::hash<float>()(value)); }
auto Hasher::Add(double value) -> void { Add(std::hash<double>()(value)); }

auto Hasher::Add(const char *str) -> void {
  Add(std::hash<std::string_view>()(str));
}

auto Hasher::Get() const -> size_t { return hashedValue; }
auto Hasher::Reset() -> void { hashedValue = 0; }

} // namespace Hash