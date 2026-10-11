#pragma once

#include <cstdint>

namespace Units::Memory {

auto constexpr BytesToKiB(uint64_t bytes) -> uint64_t {
  return bytes / 1024ULL;
}
auto constexpr BytesToMiB(uint64_t bytes) -> uint64_t {
  return bytes / (1024ULL * 1024ULL);
}
auto constexpr BytesToGiB(uint64_t bytes) -> uint64_t {
  return bytes / (1024ULL * 1024ULL * 1024ULL);
}

auto constexpr KiBToBytes(uint64_t bytes) -> uint64_t {
  return bytes * 1024ULL;
}
auto constexpr MiBToBytes(uint64_t bytes) -> uint64_t {
  return bytes * (1024ULL * 1024ULL);
}
auto constexpr GiBToBytes(uint64_t bytes) -> uint64_t {
  return bytes * (1024ULL * 1024ULL * 1024ULL);
}

auto constexpr KiBToMiB(uint64_t bytes) -> uint64_t { return bytes / 1024ULL; }

auto constexpr KiBToGiB(uint64_t bytes) -> uint64_t {
  return bytes / (1024ULL * 1024ULL);
}

auto constexpr MiBToKiB(uint64_t bytes) -> uint64_t { return bytes * 1024ULL; }

auto constexpr GiBToKiB(uint64_t bytes) -> uint64_t {
  return bytes * (1024ULL * 1024ULL);
}

auto constexpr MiBToGiB(uint64_t bytes) -> uint64_t { return bytes / 1024ULL; }
auto constexpr GiBToMiB(uint64_t bytes) -> uint64_t { return bytes * 1024ULL; }

} // namespace Units::Memory
