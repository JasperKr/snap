#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
namespace Data {

static const Type LuaBytedataType = Type("Bytedata");

struct ByteData : Object {
public:
  static auto GetType() -> Type const * { return &LuaBytedataType; }

  explicit ByteData(size_t size) : size(size), data(new uint8_t[size]) {}

  ByteData(const uint8_t *src, size_t size) : size(size), data(new uint8_t[size]) { std::memcpy(data, src, size); }

  ByteData(const std::byte *src, size_t size) : size(size), data(new uint8_t[size]) { std::memcpy(data, src, size); }

  ByteData(const ByteData &other) = delete; // disable copy constructor
  ByteData(ByteData &&other) = delete;
  auto operator=(const ByteData &other) -> ByteData & = delete;
  auto operator=(ByteData &&other) noexcept -> ByteData & = delete;

  ~ByteData() override {
    if (parent != nullptr) {
      parent->release(); // Parent owns the data
    } else {
      delete[] data; // We own the data
    }
  }

  [[nodiscard]] auto GetSize() const -> size_t { return size; }
  auto GetData() -> uint8_t * { return data; }
  [[nodiscard]] auto GetData() const -> const uint8_t * { return data; }
  [[nodiscard]] auto GetDataSpan() const -> std::span<uint8_t> { return {data, size}; }

  template <typename T> constexpr auto GetDataSpan() -> std::span<T> {
    // NOLINTNEXTLINE
    return {reinterpret_cast<T *>(data), size / sizeof(T)};
  }

  [[nodiscard]] auto Clone() const -> ByteData { return {data, size}; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override { return ByteData::GetType(); }

  [[nodiscard]] auto GetParent() const -> ByteData * { return parent; }

  [[nodiscard]] auto View(size_t offset, size_t range) -> Result<Ref<ByteData>>;

  // private constructor for views
  ByteData(uint8_t *data, size_t size, ByteData *parent) : size(size), data(data), parent(parent) {}

private:
  size_t size = 0;
  uint8_t *data = nullptr;

  ByteData *parent = nullptr;
};

} // namespace Data