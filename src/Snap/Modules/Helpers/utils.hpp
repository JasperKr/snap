#pragma once

#include "Modules/error.hpp"
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <execution>
#include <initializer_list>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace Utils {

// Unordered erase from vector, does not preserve order but is O(1) (at least, for remove cost, iterating is still O(n))
// If predicate returns true, the element is removed
template <typename T, typename Pred>
  requires std::is_invocable_r_v<bool, Pred, T &>
auto UnorderedErase(std::vector<T> &vect, Pred &&predicate) -> uint32_t {
  uint32_t count = 0;
  for (std::size_t i = 0; i < vect.size();) {
    if (std::forward<Pred>(predicate)(vect[i])) {
      vect[i] = std::move(vect.back());
      vect.pop_back();
      count++;
    } else {
      i++;
    }
  }

  return count;
}

template <typename T, typename V>
  requires std::equality_comparable_with<T, V>
auto UnorderedErase(std::vector<T> &vect, const V &value) -> uint32_t {
  uint32_t count = 0;

  for (std::size_t i = 0; i < vect.size();) {
    if (vect[i] == value) {
      vect[i] = std::move(vect.back());
      vect.pop_back();
      count++;
    } else {
      i++;
    }
  }

  return count;
}

template <typename T>
auto UnorderedErase(std::vector<T> &vect, std::size_t index) -> uint32_t {
  if (index >= vect.size()) {
    return 0;
  }
  vect[index] = std::move(vect.back());
  vect.pop_back();

  return 1;
}

auto InterleaveSpans(
    std::vector<std::pair<std::span<const uint8_t>, size_t>> &spans)
    -> std::vector<uint8_t>;

auto AlignUp(size_t value, size_t alignment) -> size_t;
auto AlignDown(size_t value, size_t alignment) -> size_t;

auto Subspan(std::span<const uint8_t> span, size_t offset, size_t size)
    -> std::span<const uint8_t>;
auto Subspan(std::vector<uint8_t> &vec, size_t offset, size_t size)
    -> std::span<uint8_t>;

auto SetBindingToSlot(uint32_t set, uint32_t binding) -> uint64_t;

auto SlotToSetBinding(uint64_t slot) -> std::pair<uint32_t, uint32_t>;

template <typename T, typename Comp_lt, typename Comp_Eq>
auto DeDuplicate(std::vector<T> &data,
                 Comp_lt compare_less_than = std::less<T>(),
                 Comp_Eq compare_equals = std::equal_to<T>()) {
  std::ranges::sort(data, compare_less_than);
  auto [first, last] = std::ranges::unique(data, compare_equals);
  data.erase(first, last);
}

template <class T, class F> constexpr void ForEachBit(T mask, F &&func) {
  static_assert(std::is_unsigned_v<T>, "mask must be unsigned");
  while (mask) {
    unsigned bit = std::countr_zero(mask);
    std::forward<F>(func)(bit);
    mask &= (mask - 1);
  }
}

// Returns the index of each set bit in the mask
// 23 -> 0, 1, 2, 4
template <class T> struct BitIndexRange {
  T mask;
  struct it {
    T m;
    auto operator*() const -> unsigned { return std::countr_zero(m); }
    auto operator++() -> it & {
      m &= (m - 1);
      return *this;
    }
    auto operator!=(const it &obj) const -> bool { return m != obj.m; }
  };
  auto begin() const -> it { return {mask}; }
  auto end() const -> it { return {0}; }
};

// Returns each set bit in the mask as a value with only that bit set
// 23 -> 1, 2, 4, 16
template <class T> struct BitMaskRange {
  T mask;
  struct it {
    T m;
    auto operator*() const -> T { return T(1) << std::countr_zero(m); }
    auto operator++() -> it & {
      m &= (m - 1);
      return *this;
    }
    auto operator!=(const it &obj) const -> bool { return m != obj.m; }
  };
  auto begin() const -> it { return {mask}; }
  auto end() const -> it { return {0}; }
};

auto Includes(uint64_t src, uint64_t flag) -> bool;

auto Excludes(uint64_t src, uint64_t flag) -> bool;

auto GetMemoryUsage() -> size_t;

template <typename T> struct EnumStringHelper {
  std::unordered_map<T, std::string> enumToString;
  std::unordered_map<std::string, T> stringToEnum;

  EnumStringHelper(std::initializer_list<std::pair<std::string, T>> entries) {
    for (const auto &[key, value] : entries) {
      enumToString[value] = key;
      stringToEnum[key] = value;
    }
  }

  explicit EnumStringHelper(std::vector<std::pair<std::string, T>> entries) {
    for (const auto &[key, value] : entries) {
      enumToString[value] = key;
      stringToEnum[key] = value;
    }
  }

  explicit EnumStringHelper(const std::vector<std::string> &keys) {
    for (size_t i = 0; i < keys.size(); ++i) {
      enumToString[static_cast<T>(i)] = keys[i];
      stringToEnum[keys[i]] = static_cast<T>(i);
    }
  }

  auto ToString(T value) const -> std::string_view {
    auto iter = enumToString.find(value);
    if (iter == enumToString.end()) {
      return "UNKNOWN";
    }
    return iter->second;
  }

  [[nodiscard]] auto FromString(const std::string_view &str) const -> T {
    auto iter = stringToEnum.find(str);
    if (iter == stringToEnum.end()) {
      return T{};
    }
    return iter->second;
  }

  [[nodiscard]] auto FromString(const std::string_view &str,
                                T defaultValue) const -> T {
    auto iter = stringToEnum.find(str);
    if (iter == stringToEnum.end()) {
      return defaultValue;
    }
    return iter->second;
  }
};

// Fast ceil division, returns the smallest integer greater than or equal to value/divisor
template <typename T> constexpr auto CeilDivFast(T value, T divisor) -> T {
  return (value + divisor - 1) / divisor;
}

// Overflow-safe ceil division, returns the smallest integer greater than or equal to value/divisor
template <typename T> constexpr auto CeilDiv(T value, T divisor) -> T {
  return (value / divisor) + (value % divisor != 0);
}

template <typename F> void ParallelFor(size_t count, F &&func) {
  std::vector<size_t> indices(count);
  std::ranges::iota(indices, 0);

  std::for_each(std::execution::par, indices.begin(), indices.end(),
                std::forward<F>(func));
}

template <typename T, typename F>
  requires(std::same_as<std::invoke_result_t<F &, T &>, void>)
void ParallelFor(std::vector<T> &data, F &&func) {
  std::vector<size_t> indices(data.size());
  std::ranges::iota(indices, 0);

  std::for_each(std::execution::par, indices.begin(), indices.end(),
                [&](const size_t index) -> void {
                  std::forward<F>(func)(data.at(index));
                });
}

template <typename F>
  requires(std::same_as<std::invoke_result_t<F &, size_t>, Error>)
auto ParallelFor(size_t count, F &&func) -> Error {
  std::vector<size_t> indices(count);
  std::ranges::iota(indices, 0);

  std::atomic<bool> hasError{false};
  auto error = Error::Success();

  std::for_each(std::execution::par, indices.begin(), indices.end(),
                [&](const auto &index) -> void {
                  if (hasError.load()) {
                    return;
                  }

                  auto err = std::forward<F>(func)(index);

                  if (Error::IsError(err)) {
                    if (!hasError.exchange(true, std::memory_order_relaxed)) {
                      error = err;
                    }
                  }
                });

  return error;
}

// Returns the next capacity that is greater than or equal to requiredCapacity
// and is a power-of-two multiple of currentCapacity.
auto NextCapacity(size_t currentCapacity, size_t requiredCapacity) -> size_t;

// Returns the next capacity that is greater than or equal to requiredCapacity
// and is a power-of-two multiple of currentCapacity.
auto NextCapacity(size_t currentCapacity, size_t requiredCapacity,
                  size_t initialCapacity) -> size_t;

// NOLINTBEGIN
template <typename F> struct Defer {
  F func;

  Defer(const Defer &) = delete;
  Defer(Defer &&) = delete;
  auto operator=(const Defer &) -> Defer & = delete;
  auto operator=(Defer &&) -> Defer & = delete;
  explicit Defer(F &&func) : func(std::forward<F>(func)) {}
  ~Defer() { func(); }
};

template <typename F> Defer(F) -> Defer<F>;

#define CONCAT_IMPL(a, b) a##b
#define CONCAT(a, b) CONCAT_IMPL(a, b)

#define UNIQUE_NAME(prefix) CONCAT(prefix, __COUNTER__)

#define snap_defer(...)                                                        \
  auto UNIQUE_NAME(_defer_) = ::Utils::Defer([&] { __VA_ARGS__; })

#if defined(__clang__) || defined(__GNUC__)
#define ASSUME(x)                                                              \
  do {                                                                         \
    if (!(x))                                                                  \
      __builtin_unreachable();                                                 \
  } while (0)
#elif defined(_MSC_VER)
#define ASSUME(x) __assume(x)
#endif

// NOLINTEND

} // namespace Utils