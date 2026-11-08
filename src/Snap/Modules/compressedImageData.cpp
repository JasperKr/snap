#include "compressedImageData.hpp"
#include "Modules/console.hpp"
#include "Modules/dds.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/image.hpp"
#include <vector>

namespace Image {
auto CompressedImageData::Create(VkExtent2D dimensions, VkFormat format) -> Result<Ref<CompressedImageData>> {
  assert(Graphics::Format::GetSize(format) != 0);
  return Ref<CompressedImageData>::Make(dimensions, format);
}

auto CompressedImageData::Create(VkExtent2D dimensions, const std::span<uint8_t> &srcData, VkFormat format) -> Result<Ref<CompressedImageData>> {
  assert(Graphics::Format::GetSize(format) != 0);
  auto imgdata = Ref<CompressedImageData>::Make(dimensions, format);
  std::memcpy(imgdata->GetDataPtr(), srcData.data(), srcData.size());
  return imgdata;
}

auto CompressedImageData::Create(VkExtent2D dimensions, Data::ByteData &byteData, VkFormat format) -> Result<Ref<CompressedImageData>> {
  size_t dataSize = static_cast<size_t>(dimensions.width) * static_cast<size_t>(dimensions.height) * Graphics::Format::GetSize(format);

  if (byteData.GetSize() != dataSize && dataSize > 0) {
    return Error::Unexpected("ByteData size does not match image dimensions.");
  }

  return Ref<CompressedImageData>::Make(dimensions, byteData, format);
}

auto CompressedImageData::Create(const std::string &filepath) -> Result<Ref<CompressedImageData>> {

  auto fileLoadResult = Filesystem::ReadFile(filepath);

  if (Error::IsError(fileLoadResult)) {
    return fileLoadResult.error();
  }

  auto filedata = fileLoadResult.value();

  auto bytedata = Data::ByteData(filedata.size());
  std::memcpy(bytedata.GetData(), filedata.data(), filedata.size());

  return Create(bytedata);
}

auto CompressedImageData::Create(const std::span<uint8_t> &data) -> Result<Ref<CompressedImageData>> {
  if (!Image::IsDDS(data)) {
    return Error::Unexpected("Unsupported image format.");
  }

  constexpr size_t MAGIC_SIZE = 4;
  constexpr size_t HEADER_SIZE = MAGIC_SIZE + sizeof(DDS_HEADER);

  DDS_HEADER header{};
  memcpy(&header, data.data() + MAGIC_SIZE, sizeof(DDS_HEADER)); // NOLINT

  if (header.size != sizeof(DDS_HEADER)) {
    return Error::Unexpected("Invalid DDS header size.");
  }

  auto format = DDSFourCCToVkFormat(header.ddspf.fourCC);

  const auto fourCCStr = std::string_view( // NOLINTNEXTLINE
      reinterpret_cast<const char *>(&header.ddspf.fourCC), MAGIC_SIZE);
  if (format != VK_FORMAT_UNDEFINED) {
    if (data.size() < HEADER_SIZE) {
      return Error::Unexpected("DDS data is too small for headers.");
    }

    size_t pixelDataSize = data.size() - HEADER_SIZE;
    VkExtent2D size = {header.width, header.height};

    return CompressedImageData::Create(size, data, format);
  }

  if (header.ddspf.fourCC != MakeFourCC('D', 'X', '1', '0')) {
    return Error::Unexpected("Unsupported DDS format (missing DX10 header).");
  }

  DDS_HEADER_DXT10 header10{};
  memcpy(&header10, data.data() + HEADER_SIZE, // NOLINT
         sizeof(DDS_HEADER_DXT10));

  auto formatResult = DXgiDDSFormatToVkFormat(header10.dxgiFormat);

  if (formatResult == VK_FORMAT_UNDEFINED) {
    return Error::Unexpected("Unsupported DDS format (unsupported DXGI format).");
  }

  format = formatResult;

  size_t headerSize = HEADER_SIZE + sizeof(DDS_HEADER_DXT10);
  if (data.size() < headerSize) {
    return Error::Unexpected("DDS data is too small for headers.");
  }

  VkExtent2D size = {header.width, header.height};

  const auto &imgdata = CHECK_RES(CompressedImageData::Create(size, data.subspan(headerSize), format));

  imgdata->mipmapCount = static_cast<int>(header.mipMapCount);

  return imgdata;
}

auto CompressedImageData::Create(const Data::ByteData &byteData) -> Result<Ref<CompressedImageData>> { return Create(byteData.GetDataSpan()); }

} // namespace Image
