#include "bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"

namespace Data {

[[nodiscard]] auto ByteData::View(size_t offset, size_t range) -> Result<Ref<ByteData>> {
  if (offset + range > size) {
    return Error::Unexpected("ByteData view out of bounds.");
  }
  this->retain(); // Retained by the view

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return Ref<ByteData>::Make(data + offset, range, this);
}

ByteData::~ByteData() {
  if (parent != nullptr) {
    parent->release(); // Parent owns the data
  } else {
    delete[] data; // We own the data
  }
}

ByteData::ByteData(size_t size) : size(size), data(new uint8_t[size]) {}

ByteData::ByteData(const uint8_t *src, size_t size) : size(size), data(new uint8_t[size]) { std::memcpy(data, src, size); }

ByteData::ByteData(const std::byte *src, size_t size) : size(size), data(new uint8_t[size]) { std::memcpy(data, src, size); }

[[nodiscard]] auto ByteData::Clone() const -> ByteData { return {data, size}; }

ByteData::ByteData(uint8_t *data, size_t size, ByteData *parent) : size(size), data(data), parent(parent) {}

} // namespace Data