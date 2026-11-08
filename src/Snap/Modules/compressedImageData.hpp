#pragma once

#include "../Graphics/format.hpp"
#include "Math/vector.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "color.hpp"
#include "image.hpp"
#include <cassert>
#include <cstddef>
#include <span>

#include "vulkan/vulkan_core.h"
#include <cstdint>
namespace Image {

static const Type LuaCompressedImageDataType = Type("CompressedImageData");

struct CompressedImageData : Object {
public:
  ~CompressedImageData() override { internalData = {}; }

  auto SetColor(Math::Uvec3 position, const Color &color) -> Error;
  auto GetColor(Math::Uvec3 position) -> Result<Color>;
  auto Copy(const CompressedImageData &source) -> void;
  auto GetDataPtr() -> uint8_t * { return internalData->GetData(); }
  auto GetSpan() -> std::span<uint8_t> { return {internalData->GetData(), internalData->GetSize()}; }
  auto GetSpan() const -> std::span<const uint8_t> { return {internalData->GetData(), internalData->GetSize()}; }
  auto GetMipmapCount() const -> int { return mipmapCount; }
  auto GetMipSize(int mipmap) const -> size_t {
    auto size = GetDimensions(mipmap);
    auto blocksX = (size.width + 3) / 4;
    auto blocksY = (size.height + 3) / 4;
    return static_cast<size_t>(blocksX) * static_cast<size_t>(blocksY) * Graphics::Format::GetSize(format);
  }
  auto GetMipSpan(int mipmap) const -> std::span<uint8_t> {
    assert(mipmap >= 0 && mipmap < mipmapCount);
    size_t offset = 0;
    for (int i = 0; i < mipmap; ++i) {
      offset += GetMipSize(i);
    }
    // NOLINTNEXTLINE
    return {internalData->GetData() + offset, GetMipSize(mipmap)};
  }
  auto GetMipPtr(int mipmap) const -> uint8_t * {
    assert(mipmap >= 0 && mipmap < mipmapCount);
    size_t offset = 0;
    for (int i = 0; i < mipmap; ++i) {
      offset += GetMipSize(i);
    }
    // NOLINTNEXTLINE
    return internalData->GetData() + offset;
  }
  [[nodiscard]] auto GetSize() const -> size_t { return internalData->GetSize(); }
  [[nodiscard]] auto GetWidth() const -> uint32_t { return size.width; }
  [[nodiscard]] auto GetHeight() const -> uint32_t { return size.height; }
  [[nodiscard]] auto GetDimensions() const -> VkExtent2D { return size; }
  [[nodiscard]] auto GetDimensions(int miplevel) const -> VkExtent2D { return Image::GetDimensions(size, miplevel); }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }
  [[nodiscard]] auto GetChannelCount() const -> uint32_t { return Graphics::Format::GetChannelCount(format); }
  [[nodiscard]] auto GetFormatSize() const -> size_t { return Graphics::Format::GetSize(format); }

  CompressedImageData(const CompressedImageData &) = delete;
  CompressedImageData(CompressedImageData &&) = delete;
  auto operator=(const CompressedImageData &) -> CompressedImageData & = delete;
  auto operator=(CompressedImageData &&) -> CompressedImageData & = delete;
  // NOLINTNEXTLINE
  CompressedImageData(VkExtent2D dimensions, Data::ByteData &byteData, VkFormat format) : internalData(&byteData), size(dimensions), format(format) {}
  // NOLINTNEXTLINE
  CompressedImageData(VkExtent2D dimensions, VkFormat format)
      : internalData(Ref<Data::ByteData>::Make(static_cast<size_t>(dimensions.width * dimensions.height * Graphics::Format::GetSize(format)))),
        size(dimensions), format(format) {}

  static auto Create(VkExtent2D dimensions, VkFormat format) -> Result<Ref<CompressedImageData>>;
  static auto Create(VkExtent2D dimensions, const std::span<uint8_t> &srcData, VkFormat format) -> Result<Ref<CompressedImageData>>;
  static auto Create(VkExtent2D dimensions, Data::ByteData &byteData, VkFormat format) -> Result<Ref<CompressedImageData>>;
  static auto Create(const std::string &filepath) -> Result<Ref<CompressedImageData>>;
  static auto Create(const Data::ByteData &byteData) -> Result<Ref<CompressedImageData>>;
  static auto Create(const std::span<uint8_t> &data) -> Result<Ref<CompressedImageData>>;

  static auto GetType() -> Type const * { return &LuaCompressedImageDataType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override { return CompressedImageData::GetType(); }

  auto GetRowPitch() const -> size_t { return size.width * static_cast<size_t>(Graphics::Format::GetSize(format)); }

  auto GetSlicePitch() const -> size_t { return GetRowPitch() * size.height; }

private:
  VkExtent2D size;
  VkFormat format;

  Ref<Data::ByteData> internalData;
  int mipmapCount{1};
};

} // namespace Image