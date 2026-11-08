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

} // namespace Data