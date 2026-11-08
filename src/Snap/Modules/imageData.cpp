#include "imageData.hpp"
#include "Graphics/format.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/bytedata.hpp"
#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include <cassert>
#include <vector>

#include "Modules/image.hpp"
#include "stb/stb_image.h"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <cstring>
#include <unordered_map>

// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTBEGIN(hicpp-no-array-decay)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
// NOLINTBEGIN(readability-magic-numbers)

// Set, Get functions for different formats
struct FormatFunctions {
  using SetFunction = void (*)(const Color &color, uint8_t *outData);
  using GetFunction = void (*)(const uint8_t *inData, Color &outColor);

  SetFunction set;
  GetFunction get;
};

static constexpr float ToUint8_t = 255.0F;
static constexpr float ToUint16_t = 65535.0F;
static constexpr float ToUint32_t = 4294967295.0F;
static constexpr float FromUint8_t = 1.0F / 255.0F;
static constexpr float FromUint16_t = 1.0F / 65535.0F;
static constexpr float FromUint32_t = 1.0F / 4294967295.0F;
// map of set and get functions for different formats
static const std::unordered_map<VkFormat, FormatFunctions> formatFunctionMap = {
    {VK_FORMAT_R8G8B8A8_UNORM,
     {// Set function
      .set = [](const Color &color, uint8_t *__restrict outData) -> void {
        outData[0] = static_cast<uint8_t>(color.r * ToUint8_t);
        outData[1] = static_cast<uint8_t>(color.g * ToUint8_t);
        outData[2] = static_cast<uint8_t>(color.b * ToUint8_t);
        outData[3] = static_cast<uint8_t>(color.a * ToUint8_t);
      },
      // Get function
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        outColor.r = static_cast<float>(inData[0]) * FromUint8_t;
        outColor.g = static_cast<float>(inData[1]) * FromUint8_t;
        outColor.b = static_cast<float>(inData[2]) * FromUint8_t;
        outColor.a = static_cast<float>(inData[3]) * FromUint8_t;
      }}},
    {VK_FORMAT_R8G8B8A8_SRGB,
     {// Set function
      .set = [](const Color &color, uint8_t *__restrict outData) -> void {
        outData[0] = static_cast<uint8_t>(color.r * ToUint8_t);
        outData[1] = static_cast<uint8_t>(color.g * ToUint8_t);
        outData[2] = static_cast<uint8_t>(color.b * ToUint8_t);
        outData[3] = static_cast<uint8_t>(color.a * ToUint8_t);
      },
      // Get function
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        outColor.r = static_cast<float>(inData[0]) * FromUint8_t;
        outColor.g = static_cast<float>(inData[1]) * FromUint8_t;
        outColor.b = static_cast<float>(inData[2]) * FromUint8_t;
        outColor.a = static_cast<float>(inData[3]) * FromUint8_t;
      }}},
    {VK_FORMAT_R8_UNORM,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void { outData[0] = static_cast<uint8_t>(color.r * ToUint8_t); },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void { outColor.r = static_cast<float>(inData[0]) * FromUint8_t; }}},
    {VK_FORMAT_R8G8_UNORM,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        outData[0] = static_cast<uint8_t>(color.r * ToUint8_t);
        outData[1] = static_cast<uint8_t>(color.g * ToUint8_t);
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        outColor.r = static_cast<float>(inData[0]) * FromUint8_t;
        outColor.g = static_cast<float>(inData[1]) * FromUint8_t;
      }}},
    {VK_FORMAT_R16G16B16A16_SFLOAT,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        auto r16 = static_cast<numeric::float16_t>(color.r);
        auto g16 = static_cast<numeric::float16_t>(color.g);
        auto b16 = static_cast<numeric::float16_t>(color.b);
        auto a16 = static_cast<numeric::float16_t>(color.a);
        std::memcpy(outData + 0, &r16, sizeof(numeric::float16_t));
        std::memcpy(outData + 2, &g16, sizeof(numeric::float16_t));
        std::memcpy(outData + 4, &b16, sizeof(numeric::float16_t));
        std::memcpy(outData + 6, &a16, sizeof(numeric::float16_t));
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        numeric::float16_t r16{};
        numeric::float16_t g16{};
        numeric::float16_t b16{};
        numeric::float16_t a16{};
        std::memcpy(&r16, inData + 0, sizeof(numeric::float16_t));
        std::memcpy(&g16, inData + 2, sizeof(numeric::float16_t));
        std::memcpy(&b16, inData + 4, sizeof(numeric::float16_t));
        std::memcpy(&a16, inData + 6, sizeof(numeric::float16_t));
        outColor.r = static_cast<float>(r16);
        outColor.g = static_cast<float>(g16);
        outColor.b = static_cast<float>(b16);
        outColor.a = static_cast<float>(a16);
      }}},
    {VK_FORMAT_R32G32B32A32_SFLOAT,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        auto r32 = static_cast<float>(color.r);
        auto g32 = static_cast<float>(color.g);
        auto b32 = static_cast<float>(color.b);
        auto a32 = static_cast<float>(color.a);
        std::memcpy(outData + 0, &r32, sizeof(float));
        std::memcpy(outData + 4, &g32, sizeof(float));
        std::memcpy(outData + 8, &b32, sizeof(float));
        std::memcpy(outData + 12, &a32, sizeof(float));
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        float r32{};
        float g32{};
        float b32{};
        float a32{};
        std::memcpy(&r32, inData + 0, sizeof(float));
        std::memcpy(&g32, inData + 4, sizeof(float));
        std::memcpy(&b32, inData + 8, sizeof(float));
        std::memcpy(&a32, inData + 12, sizeof(float));
        outColor.r = r32;
        outColor.g = g32;
        outColor.b = b32;
        outColor.a = a32;
      }}},
    {VK_FORMAT_R32G32B32_SFLOAT,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        auto r32 = static_cast<float>(color.r);
        auto g32 = static_cast<float>(color.g);
        auto b32 = static_cast<float>(color.b);
        std::memcpy(outData + 0, &r32, sizeof(float));
        std::memcpy(outData + 4, &g32, sizeof(float));
        std::memcpy(outData + 8, &b32, sizeof(float));
      },
      .get = [](const uint8_t *inData, Color &outColor) -> void {
        float r32{};
        float g32{};
        float b32{};
        std::memcpy(&r32, inData + 0, sizeof(float));
        std::memcpy(&g32, inData + 4, sizeof(float));
        std::memcpy(&b32, inData + 8, sizeof(float));
        outColor.r = r32;
        outColor.g = g32;
        outColor.b = b32;
      }}},
    {VK_FORMAT_R32_SFLOAT,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        auto r32 = static_cast<float>(color.r);
        std::memcpy(outData + 0, &r32, sizeof(float));
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        float r32{};
        std::memcpy(&r32, inData + 0, sizeof(float));
        outColor.r = r32;
      }}},
    {VK_FORMAT_R4G4B4A4_UNORM_PACK16,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        uint16_t packed = (static_cast<uint32_t>(color.r * 15.0F) << 12U) | (static_cast<uint32_t>(color.g * 15.0F) << 8U) |
                          (static_cast<uint32_t>(color.b * 15.0F) << 4U) | (static_cast<uint32_t>(color.a * 15.0F) << 0U);
        std::memcpy(outData, &packed, sizeof(uint16_t));
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        uint32_t packed{};
        std::memcpy(&packed, inData, sizeof(uint16_t));
        outColor.r = static_cast<float>((packed >> 12U) & 0x0FU) / 15.0F;
        outColor.g = static_cast<float>((packed >> 8U) & 0x0FU) / 15.0F;
        outColor.b = static_cast<float>((packed >> 4U) & 0x0FU) / 15.0F;
        outColor.a = static_cast<float>((packed >> 0U) & 0x0FU) / 15.0F;
      }}},
    {VK_FORMAT_R5G5B5A1_UNORM_PACK16,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        uint16_t packed = (static_cast<uint32_t>(color.r * 31.0F) << 11U) | (static_cast<uint32_t>(color.g * 31.0F) << 6U) |
                          (static_cast<uint32_t>(color.b * 31.0F) << 1U) | (static_cast<uint32_t>(color.a * 1.0F) << 0U);
        std::memcpy(outData, &packed, sizeof(uint16_t));
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        uint32_t packed{};
        std::memcpy(&packed, inData, sizeof(uint16_t));
        outColor.r = static_cast<float>((packed >> 11U) & 0x1FU) / 31.0F;
        outColor.g = static_cast<float>((packed >> 6U) & 0x1FU) / 31.0F;
        outColor.b = static_cast<float>((packed >> 1U) & 0x1FU) / 31.0F;
        outColor.a = static_cast<float>((packed >> 0U) & 0x01U) / 1.0F;
      }}},
    {VK_FORMAT_B10G11R11_UFLOAT_PACK32,
     {.set = [](const Color &color, uint8_t *__restrict outData) -> void {
        uint32_t r11 = Math::Float11::fromFloat(color.r).bits;
        uint32_t g11 = Math::Float11::fromFloat(color.g).bits;
        uint32_t b10 = Math::Float10::fromFloat(color.b).bits;
        uint32_t packed = (b10 << 22U) | (g11 << 11U) | (r11 << 0U);
        std::memcpy(outData, &packed, sizeof(uint32_t));
      },
      .get = [](const uint8_t *__restrict inData, Color &outColor) -> void {
        uint32_t packed{};
        std::memcpy(&packed, inData, sizeof(uint32_t));
        auto r11 = (packed >> 0U) & 0x3FFU;
        auto g11 = (packed >> 11U) & 0x7FFU;
        auto b10 = (packed >> 22U) & 0x3FFU;
        outColor.r = Math::Float11::toFloat(Math::Float11::value(r11));
        outColor.g = Math::Float11::toFloat(Math::Float11::value(g11));
        outColor.b = Math::Float10::toFloat(Math::Float10::value(b10));
      }}}};

namespace Image {
auto ImageData::SetColor(Math::Uvec3 position, const Color &color) -> Error {
  size_t index = (GetSlicePitch() * position.z) + (GetRowPitch() * position.y) + (position.x * GetFormatSize());

  auto funcIterator = formatFunctionMap.find(format);
  if (funcIterator != formatFunctionMap.end()) {
    const FormatFunctions &functions = funcIterator->second;
    functions.set(color, &GetDataPtr()[index]);
    return Error::Success();
  }

  return Error::Create("Unsupported image format.");
}

auto ImageData::GetColor(Math::Uvec3 position) -> Result<Color> {
  size_t index = (GetSlicePitch() * position.z) + (GetRowPitch() * position.y) + (position.x * GetFormatSize());

  auto funcIterator = formatFunctionMap.find(format);
  static Color outColor; // NOLINT
  if (funcIterator != formatFunctionMap.end()) {
    const FormatFunctions &functions = funcIterator->second;
    functions.get(&GetDataPtr()[index], outColor);
    return outColor;
  }

  // Unsupported format, handle error as needed
  return Error::Create("Unsupported image format.");
}

auto ImageData::Create(VkExtent3D dimensions, VkFormat format) -> Result<Ref<ImageData>> {
  assert(Graphics::Format::GetSize(format) != 0);
  return Ref<ImageData>::Make(dimensions, format);
}

auto ImageData::Create(VkExtent3D dimensions, const std::span<uint8_t> &srcData, VkFormat format) -> Result<Ref<ImageData>> {
  assert(Graphics::Format::GetSize(format) != 0);
  auto imgdata = Ref<ImageData>::Make(dimensions, format);
  std::memcpy(imgdata->GetDataPtr(), srcData.data(), srcData.size());
  return imgdata;
}

auto ImageData::Create(VkExtent3D dimensions, Data::ByteData &byteData, VkFormat format) -> Result<Ref<ImageData>> {
  size_t dataSize = static_cast<size_t>(dimensions.width) * static_cast<size_t>(dimensions.height) * static_cast<size_t>(dimensions.depth) *
                    Graphics::Format::GetSize(format);

  if (byteData.GetSize() != dataSize && dataSize > 0) {
    return Error::Unexpected("ByteData size does not match image dimensions.");
  }

  return Ref<ImageData>::Make(dimensions, byteData, format);
}

auto ImageData::Create(const std::string &filepath) -> Result<Ref<ImageData>> {

  auto fileLoadResult = Filesystem::ReadFile(filepath);

  if (Error::IsError(fileLoadResult)) {
    return fileLoadResult.error();
  }

  auto filedata = fileLoadResult.value();

  auto bytedata = Data::ByteData(filedata.size());
  std::memcpy(bytedata.GetData(), filedata.data(), filedata.size());

  return Create(bytedata);
}

auto ImageData::Create(const std::span<uint8_t> &data) -> Result<Ref<ImageData>> {
  // Allowed formats: jpeg, png, tga, bmp, psd, gif, hdr, pic, ppm

  int texWidth = 0;
  int texHeight = 0;
  int texChannels = 0;
  VkFormat format{};

  if (IsDDS(data)) {
    return Error::Create("Use CompressedImageData for DDS files.");
  }

  // check for LDR formats, supported by default stbi_load
  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) == 0) {
    stbi_uc *pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load rgba8 imageData.");
    }

    auto span = std::span<uint8_t>(pixels, static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight) * 4);

    auto imageData = Image::ImageData::Create({static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1}, span, VK_FORMAT_R8G8B8A8_UNORM);

    stbi_image_free(pixels);

    if (Error::IsError(imageData)) {
      return imageData.error();
    }

    return imageData.value();
  }

  if (stbi_is_hdr_from_memory(data.data(), static_cast<int>(data.size())) != 0) {
    float *__restrict pixels = stbi_loadf_from_memory(data.data(), static_cast<int>(data.size()), &texWidth, &texHeight, &texChannels, STBI_rgb);

    if (pixels == nullptr) {
      return Error::Unexpected("Failed to load b10gr11f imageData.");
    }

    auto imageData =
        CHECK_RES(Image::ImageData::Create({static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1}, VK_FORMAT_B10G11R11_UFLOAT_PACK32));
    auto *__restrict ptr = reinterpret_cast<uint32_t *>(imageData->GetDataPtr()); // NOLINT

    size_t iterCount = static_cast<size_t>(texWidth) * static_cast<size_t>(texHeight);
    size_t readIdx = 0;

#pragma unroll 4
    for (int i = 0; i < iterCount; i++) {
      uint32_t lowp_red = Math::Float11::fromFloat(pixels[readIdx++]).bits;
      uint32_t lowp_green = Math::Float11::fromFloat(pixels[readIdx++]).bits;
      uint32_t lowp_blue = Math::Float10::fromFloat(pixels[readIdx++]).bits;

      ptr[i] = (lowp_blue << 22U) | (lowp_green << 11U) | lowp_red;
    }

    stbi_image_free(pixels);

    return imageData;
  }

  return Error::Unexpected("Unsupported image format.");
}

auto ImageData::Create(const Data::ByteData &byteData) -> Result<Ref<ImageData>> { return Create(byteData.GetDataSpan()); }

} // namespace Image

// NOLINTEND(readability-magic-numbers)
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTEND(hicpp-no-array-decay)
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
// NOLINTEND(cppcoreguidelines-pro-type-union-access)