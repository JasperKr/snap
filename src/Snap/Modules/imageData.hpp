#pragma once

#include "../Graphics/format.hpp"
#include "Math/vector.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "color.hpp"
#include <cassert>
#include <cstddef>
#include <span>

#include "vulkan/vulkan_core.h"
#include <cstdint>
namespace Image {

static const Type LuaImageDataType = Type("ImageData");

struct ImageData : Object {
public:
  ~ImageData() override { internalData = {}; }

  auto SetColor(Math::Uvec3 position, const Color &color) -> Error;
  auto GetColor(Math::Uvec3 position) -> Result<Color>;
  auto Copy(const ImageData &source) -> void;
  auto GetDataPtr() -> uint8_t * { return internalData->GetData(); }
  auto GetSpan() -> std::span<uint8_t> { return {internalData->GetData(), internalData->GetSize()}; }
  [[nodiscard]] auto GetSize() const -> size_t { return internalData->GetSize(); }
  [[nodiscard]] auto GetWidth() const -> uint32_t { return size.width; }
  [[nodiscard]] auto GetHeight() const -> uint32_t { return size.height; }
  [[nodiscard]] auto GetDepth() const -> uint32_t { return size.depth; }
  [[nodiscard]] auto GetDimensions() const -> VkExtent3D { return size; }
  [[nodiscard]] auto GetFormat() const -> VkFormat { return format; }
  [[nodiscard]] auto GetChannelCount() const -> uint32_t { return Graphics::Format::GetChannelCount(format); }
  [[nodiscard]] auto GetFormatSize() const -> size_t { return Graphics::Format::GetSize(format); }

  ImageData(const ImageData &) = delete;
  ImageData(ImageData &&) = delete;
  auto operator=(const ImageData &) -> ImageData & = delete;
  auto operator=(ImageData &&) -> ImageData & = delete;
  // NOLINTNEXTLINE
  ImageData(VkExtent3D dimensions, Data::ByteData &byteData, VkFormat format) : internalData(&byteData), size(dimensions), format(format) {}
  // NOLINTNEXTLINE
  ImageData(VkExtent3D dimensions, VkFormat format)
      : internalData(
            Ref<Data::ByteData>::Make(static_cast<size_t>(dimensions.width * dimensions.height * dimensions.depth * Graphics::Format::GetSize(format)))),
        size(dimensions), format(format) {}

  static auto Create(VkExtent3D dimensions, VkFormat format) -> Result<Ref<ImageData>>;
  static auto Create(VkExtent3D dimensions, const std::span<uint8_t> &srcData, VkFormat format) -> Result<Ref<ImageData>>;
  static auto Create(VkExtent3D dimensions, Data::ByteData &byteData, VkFormat format) -> Result<Ref<ImageData>>;
  static auto Create(const std::string &filepath) -> Result<Ref<ImageData>>;
  static auto Create(const Data::ByteData &byteData) -> Result<Ref<ImageData>>;
  static auto Create(const std::span<uint8_t> &data) -> Result<Ref<ImageData>>;

  static auto GetType() -> Type const * { return &LuaImageDataType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override { return ImageData::GetType(); }

  auto GetRowPitch() const -> size_t { return size.width * static_cast<size_t>(Graphics::Format::GetSize(format)); }

  auto GetSlicePitch() const -> size_t { return GetRowPitch() * size.height; }

private:
  VkExtent3D size;
  VkFormat format;

  Ref<Data::ByteData> internalData;
};

} // namespace Image