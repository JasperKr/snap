#include "gltfLoader.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/color.hpp"
#include "Modules/compressedImageData.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/Geometry/levelOfDetail.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/node.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "material.hpp"
#include "mikkTSpace.hpp"      // Don't remove either
#include "simdjson/simdjson.h" // <-- Do not remove; Forces the use of project simdjson instead of system simdjson
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <execution>
#include <flecs.h>
#include <mutex>
#include <numeric>
#include <optional>
#include <public/tracy/Tracy.hpp>
#include <span>
#include <string>

#include "fastgltf/include/fastgltf/core.hpp"
#include "fastgltf/include/fastgltf/types.hpp"

#include "meshoptimizer.h"
#include "vulkan/vulkan_core.h"

#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace glTF {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<std::vector<std::uint8_t>> Buffers;
std::unordered_map<std::string, std::vector<std::uint8_t>> URICache;
std::mutex URICacheMutex{};
std::unordered_map<std::string, uint16_t> NameDuplicateCount;
std::vector<
    std::variant<Ref<Image::ImageData>, Ref<Image::CompressedImageData>>>
    ImageCache;
std::vector<Ref<Graphics::Texture>> TextureCache;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

constexpr fastgltf::Extensions Extensions =
    fastgltf::Extensions::KHR_lights_punctual |
    fastgltf::Extensions::MSFT_texture_dds;

inline auto GetUniqueName(const std::string &baseName) -> std::string {
  auto countIter = NameDuplicateCount.find(baseName);
  uint16_t count = 0;
  if (countIter != NameDuplicateCount.end()) {
    count = countIter->second + 1;
    countIter->second = count;
  } else {
    NameDuplicateCount[baseName] = 0;
  }

  if (count == 0) {
    return baseName;
  }

  return baseName + "_" + std::to_string(count);
}

inline auto GetUniqueName(const char *baseName) -> std::string {
  return GetUniqueName(std::string(baseName));
}

using DataIndex = size_t;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static fastgltf::Parser Parser = fastgltf::Parser(Extensions);

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const std::string_view &basePath,
                           const fastgltf::DataSource &dataSource)
    -> Result<std::span<uint8_t>>;

inline auto LoadBufferView(const fastgltf::Asset &asset,
                           const fastgltf::BufferView &bufferView)
    -> Result<std::span<uint8_t>> {
  const auto &buffer = asset.buffers[bufferView.bufferIndex];

  const auto byteOffset = static_cast<long>(bufferView.byteOffset);
  const auto byteLength = static_cast<long>(bufferView.byteLength);

  if (byteOffset < 0 || byteLength < 0) {
    return Error::Unexpected("BufferView has negative byte offset or length.");
  }

  if (bufferView.bufferIndex < 0 ||
      static_cast<size_t>(bufferView.bufferIndex) >= asset.buffers.size()) {
    return Error::Unexpected("BufferView has invalid buffer index.");
  }

  if (bufferView.bufferIndex >= Buffers.size()) {
    return Error::Unexpected("BufferView buffer index out of bounds.");
  }

  if (static_cast<size_t>(byteOffset) + static_cast<size_t>(byteLength) >
      Buffers[bufferView.bufferIndex].size()) {
    return Error::Unexpected("BufferView byte range exceeds buffer size.");
  }

  auto &data = Buffers[bufferView.bufferIndex];

  return std::span<uint8_t>(data.data() + byteOffset, // NOLINT
                            byteLength);
}

inline auto LoadURI(const std::string_view &basePath,
                    const fastgltf::sources::URI &uriSource)
    -> Result<std::span<uint8_t>> {

  const auto &uri = uriSource.uri;
  const auto &path = Path::Join(basePath, uri.path());

  auto iter = URICache.find(path);
  if (iter != URICache.end()) {
    return iter->second;
  }

  {
    std::lock_guard<std::mutex> lock(URICacheMutex);
    auto data = CHECK_RES(Filesystem::ReadFile(path));

    URICache[path] = std::move(data);
    auto &cached = URICache[path];

    auto span = std::span<uint8_t>(cached.data(), cached.size()); // NOLINT
    return span;
  }
}

inline auto LoadDataSource(const fastgltf::Asset &asset,
                           const std::string_view &basePath,
                           const fastgltf::DataSource &dataSource)
    -> Result<std::span<uint8_t>> {
  ZoneScoped;

  // never monostate
  // std::variant<std::monostate, sources::BufferView, sources::URI, sources::Array, sources::Vector, sources::CustomBuffer, sources::ByteView, sources::Fallback>

  if (std::holds_alternative<fastgltf::sources::BufferView>(dataSource)) {
    const auto &view = std::get<fastgltf::sources::BufferView>(dataSource);
    const auto &bufferView = asset.bufferViews[view.bufferViewIndex];
    auto ret = LoadBufferView(asset, bufferView);
    return ret;
  }

  if (std::holds_alternative<fastgltf::sources::URI>(dataSource)) {
    const auto &uriSource = std::get<fastgltf::sources::URI>(dataSource);
    return LoadURI(basePath, uriSource);
  }

  if (std::holds_alternative<fastgltf::sources::Array>(dataSource)) {
    const auto &arraySource = std::get<fastgltf::sources::Array>(dataSource);

    // NOLINTBEGIN

    auto dataSpan =
        std::span<uint8_t>(reinterpret_cast<uint8_t *>(const_cast<std::byte *>(
                               arraySource.bytes.data())),
                           arraySource.bytes.size());

    // NOLINTEND

    return dataSpan;
  }
  if (std::holds_alternative<fastgltf::sources::Vector>(dataSource)) {
    const auto &vectorSource = std::get<fastgltf::sources::Vector>(dataSource);

    // NOLINTBEGIN
    auto dataSpan =
        std::span<uint8_t>(reinterpret_cast<uint8_t *>(const_cast<std::byte *>(
                               vectorSource.bytes.data())),
                           vectorSource.bytes.size());
    // NOLINTEND

    return dataSpan;
  }
  if (std::holds_alternative<fastgltf::sources::CustomBuffer>(dataSource)) {
    const auto &customBufferSource =
        std::get<fastgltf::sources::CustomBuffer>(dataSource);

    return Error::Unexpected("Custom buffers not supported.");
  }
  if (std::holds_alternative<fastgltf::sources::ByteView>(dataSource)) {
    const auto &byteViewSource =
        std::get<fastgltf::sources::ByteView>(dataSource);

    // NOLINTBEGIN

    auto dataSpan =
        std::span<uint8_t>(reinterpret_cast<uint8_t *>(const_cast<std::byte *>(
                               byteViewSource.bytes.data())),
                           byteViewSource.bytes.size());
    // NOLINTEND

    return dataSpan;
  }

  return Error::Unexpected("Unsupported data source.");
}

inline auto LoadTexture(const Graphics::GraphicsContext &context,
                        const fastgltf::Asset &asset,
                        const std::string_view &basePath,
                        const fastgltf::TextureInfo &gltfTexture)
    -> Result<Ref<Graphics::Texture>> {
  ZoneScoped;

  if (gltfTexture.textureIndex < TextureCache.size() &&
      TextureCache[gltfTexture.textureIndex].isValid()) {
    return TextureCache[gltfTexture.textureIndex];
  }

  const auto &texture = asset.textures[gltfTexture.textureIndex];
  const auto &sampler = texture.samplerIndex.has_value()
                            ? asset.samplers[texture.samplerIndex.value()]
                            : fastgltf::Sampler{};

  if (!texture.imageIndex.has_value()) {
    return Error::Unexpected("Texture has no image index.");
  }

  const auto &gltfimage = asset.images[texture.imageIndex.value()];

  auto span = CHECK_RES(LoadDataSource(asset, basePath, gltfimage.data));
  Ref<Graphics::Texture> textureRef;

  const auto &image = ImageCache[texture.imageIndex.value()];

  if (std::holds_alternative<Ref<Image::ImageData>>(image)) {
    textureRef = CHECK_RES(Graphics::Texture::FromMemory(
        context, *std::get<Ref<Image::ImageData>>(image).get(),
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        Graphics::TextureMipmapOption::Init));
  } else if (std::holds_alternative<Ref<Image::CompressedImageData>>(image)) {
    textureRef = CHECK_RES(Graphics::Texture::FromMemory(
        context, *std::get<Ref<Image::CompressedImageData>>(image).get(),
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        Graphics::TextureMipmapOption::Manual));
  } else {
    return Error::Unexpected("Invalid image type in cache.");
  }

  textureRef->SetLodRange(0.0F, (float)textureRef->GetMipmapCount() - 1);
  textureRef->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                        VK_SAMPLER_MIPMAP_MODE_LINEAR);

  if (TextureCache.size() <= gltfTexture.textureIndex) {
    TextureCache.resize(gltfTexture.textureIndex + 100UL); // NOLINT
  }

  TextureCache[gltfTexture.textureIndex] = textureRef;

  return textureRef;
}

inline auto LoadMaterial(const Graphics::GraphicsContext &context,
                         const fastgltf::Asset &asset,
                         const std::string_view &basePath,
                         const fastgltf::Material &gltfMaterial,
                         Ref<Engine::Renderer::LuaMaterial> &luaMaterial)
    -> Error {
  ZoneScoped;

  // auto &material = luaMaterial->material;
  auto *material =
      luaMaterial->entity.try_get_mut<Engine::Renderer::Material>();

  if (material == nullptr) {
    return Error::Createf(
        "Failed to get mutable reference to Material component.");
  }

  material->name = gltfMaterial.name;

  material->cullMode =
      gltfMaterial.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

  material->alphaCutoff = gltfMaterial.alphaCutoff;
  material->roughnessFactor = gltfMaterial.pbrData.roughnessFactor;
  material->metallicFactor = gltfMaterial.pbrData.metallicFactor;
  material->albedoFactor = Math::Vec4(gltfMaterial.pbrData.baseColorFactor[0],
                                      gltfMaterial.pbrData.baseColorFactor[1],
                                      gltfMaterial.pbrData.baseColorFactor[2],
                                      gltfMaterial.pbrData.baseColorFactor[3]);

  // The alpha mode is exactly the same enum as our AlphaMode.
  // But for type safety, we do a manual mapping.

  switch (gltfMaterial.alphaMode) {
  case fastgltf::AlphaMode::Opaque:
    material->alphaMode = Engine::Renderer::AlphaMode::Opaque;
    break;
  case fastgltf::AlphaMode::Mask:
    material->alphaMode = Engine::Renderer::AlphaMode::Mask;
    break;
  case fastgltf::AlphaMode::Blend:
    material->alphaMode = Engine::Renderer::AlphaMode::Blend;
    break;
  }

  material->emissiveFactor =
      Math::Vec3(gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1],
                 gltfMaterial.emissiveFactor[2]);

  if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
    material->albedoTexture =
        CHECK_RES(LoadTexture(context, asset, basePath,
                              gltfMaterial.pbrData.baseColorTexture.value()));
  }

  if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
    material->metallicRoughnessTexture = CHECK_RES(
        LoadTexture(context, asset, basePath,
                    gltfMaterial.pbrData.metallicRoughnessTexture.value()));
  }

  if (gltfMaterial.occlusionTexture.has_value()) {
    material->ambientOcclusionTexture = CHECK_RES(LoadTexture(
        context, asset, basePath, gltfMaterial.occlusionTexture.value()));
  }

  if (gltfMaterial.normalTexture.has_value()) {
    material->normalTexture = CHECK_RES(LoadTexture(
        context, asset, basePath, gltfMaterial.normalTexture.value()));
  }

  if (gltfMaterial.emissiveTexture.has_value()) {
    material->emissiveTexture = CHECK_RES(LoadTexture(
        context, asset, basePath, gltfMaterial.emissiveTexture.value()));
  }

  return Error::Success();
}

auto ComponentTypeToString(fastgltf::ComponentType type) -> std::string_view {
  switch (type) {
  case fastgltf::ComponentType::Byte:
    return "Byte";
  case fastgltf::ComponentType::UnsignedByte:
    return "UnsignedByte";
  case fastgltf::ComponentType::Short:
    return "Short";
  case fastgltf::ComponentType::UnsignedShort:
    return "UnsignedShort";
  case fastgltf::ComponentType::Int:
    return "Int";
  case fastgltf::ComponentType::UnsignedInt:
    return "UnsignedInt";
  case fastgltf::ComponentType::Float:
    return "Float";
  case fastgltf::ComponentType::Double:
    return "Double";
  default:
    return "Invalid";
  }
}

auto GetVkFormat(fastgltf::ComponentType type, int componentCount)
    -> Result<VkFormat> {

  switch (type) {
  case fastgltf::ComponentType::Byte:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R8_SINT;
    case 2:
      return VK_FORMAT_R8G8_SINT;
    case 3:
      return VK_FORMAT_R8G8B8_SINT;
    case 4:
      return VK_FORMAT_R8G8B8A8_SINT;
    default:
      return Error::Unexpected("Unsupported component count for Byte.");
    }
  case fastgltf::ComponentType::UnsignedByte:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R8_UINT;
    case 2:
      return VK_FORMAT_R8G8_UINT;
    case 3:
      return VK_FORMAT_R8G8B8_UINT;
    case 4:
      return VK_FORMAT_R8G8B8A8_UINT;
    default:
      return Error::Unexpected("Unsupported component count for UnsignedByte.");
    }
  case fastgltf::ComponentType::Short:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R16_SINT;
    case 2:
      return VK_FORMAT_R16G16_SINT;
    case 3:
      return VK_FORMAT_R16G16B16_SINT;
    case 4:
      return VK_FORMAT_R16G16B16A16_SINT;
    default:
      return Error::Unexpected("Unsupported component count for Short.");
    }
  case fastgltf::ComponentType::UnsignedShort:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R16_UINT;
    case 2:
      return VK_FORMAT_R16G16_UINT;
    case 3:
      return VK_FORMAT_R16G16B16_UINT;
    case 4:
      return VK_FORMAT_R16G16B16A16_UINT;
    default:
      return Error::Unexpected(
          "Unsupported component count for UnsignedShort.");
    }
  case fastgltf::ComponentType::Int:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R32_SINT;
    case 2:
      return VK_FORMAT_R32G32_SINT;
    case 3:
      return VK_FORMAT_R32G32B32_SINT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SINT;
    default:
      return Error::Unexpected("Unsupported component count for Int.");
    }
  case fastgltf::ComponentType::UnsignedInt:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R32_UINT;
    case 2:
      return VK_FORMAT_R32G32_UINT;
    case 3:
      return VK_FORMAT_R32G32B32_UINT;
    case 4:
      return VK_FORMAT_R32G32B32A32_UINT;
    default:
      return Error::Unexpected("Unsupported component count for UnsignedInt.");
    }
  case fastgltf::ComponentType::Float:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R32_SFLOAT;
    case 2:
      return VK_FORMAT_R32G32_SFLOAT;
    case 3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      return Error::Unexpected("Unsupported component count for Float.");
    }
  case fastgltf::ComponentType::Double:
    switch (componentCount) {
    case 1:
      return VK_FORMAT_R64_SFLOAT;
    case 2:
      return VK_FORMAT_R64G64_SFLOAT;
    case 3:
      return VK_FORMAT_R64G64B64_SFLOAT;
    case 4:
      return VK_FORMAT_R64G64B64A64_SFLOAT;
    default:
      return Error::Unexpected("Unsupported component count for Double.");
    }
  case fastgltf::ComponentType::Invalid:
  default:
    return Error::Unexpected("Unsupported or invalid component type.");
  }
}

auto ExtractVertexFormat(const fastgltf::Asset &asset,
                         const fastgltf::Primitive &primitive)
    -> Result<Graphics::VertexFormat> {
  if (primitive.attributes.empty()) {
    return Error::Unexpected("Primitive is missing attributes.");
  }

  std::vector<Graphics::VertexComponent> attributes;

  for (const auto &[semantic, accessorIndex] : primitive.attributes) {
    const fastgltf::Accessor &accessor = asset.accessors.at(accessorIndex);

    auto size = fastgltf::getComponentByteSize(accessor.componentType);
    auto count = static_cast<int>(fastgltf::getNumComponents(accessor.type));

    auto formatResult = GetVkFormat(accessor.componentType, count);

    if (Error::IsError(formatResult)) {
      return formatResult.error();
    }

    Graphics::VertexComponent attribute{};
    attribute.format = formatResult.value();
    attribute.name = semantic;
    attribute.binding = 0;
    attributes.emplace_back(attribute);
  }

  return Graphics::VertexFormat(attributes);
}

/// Normalize an integer attribute value to float [0,1] or [-1,1].
/// Writes `compCount` floats (each 4 bytes) into `dst`.
inline void NormalizeAttribute(const uint8_t *__restrict src,
                               uint8_t *__restrict dst,
                               fastgltf::ComponentType compType,
                               size_t compCount) {

  const float IntToFloat = 1.0F / 127.0F;
  const float UIntToFloat = 1.0F / 255.0F;
  const float ShortToFloat = 1.0F / 32767.0F;
  const float UShortToFloat = 1.0F / 65535.0F;

  for (size_t i = 0; i < compCount; ++i) {
    float value = 0.0F;
    switch (compType) {
    case fastgltf::ComponentType::Byte: {
      auto cvalue = static_cast<int8_t>(src[i]); // NOLINT
      value = std::max(static_cast<float>(cvalue) * IntToFloat, -1.0F);
      break;
    }
    case fastgltf::ComponentType::UnsignedByte: {
      value = static_cast<float>(src[i]) * UIntToFloat; // NOLINT
      break;
    }
    case fastgltf::ComponentType::Short: {
      int16_t cvalue{};
      memcpy(&cvalue, src + (i * 2), 2); // NOLINT
      value = std::max(static_cast<float>(cvalue) * ShortToFloat, -1.0F);
      break;
    }
    case fastgltf::ComponentType::UnsignedShort: {
      uint16_t cvalue{};
      memcpy(&cvalue, src + (i * 2), 2); // NOLINT
      value = static_cast<float>(cvalue) * UShortToFloat;
      break;
    }
    default:
      break;
    }
    memcpy(dst + (i * sizeof(float)), &value, sizeof(float)); // NOLINT
  }
}

static inline auto PackToSigned10Bit(float value) -> uint32_t {
  value = std::max(std::min(value, 1.0F), -1.0F);
  value = (value * 0.5F) + 0.5F; // Map from [-1,1] to [0,1] NOLINT
  auto intValue = static_cast<int32_t>(std::round(value * 1023.0F)); // NOLINT
  return static_cast<uint32_t>(intValue) & 0x3FF;                    // NOLINT
}

static void ConvertNormalToPacked10Bit(const uint8_t *__restrict src,
                                       uint8_t *__restrict dst) {
  float normalX = 0.0F;
  float normalY = 0.0F;
  float normalZ = 0.0F;
  memcpy(&normalX, src, sizeof(float));                       // NOLINT
  memcpy(&normalY, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&normalZ, src + (2 * sizeof(float)), sizeof(float)); // NOLINT

  normalZ = -normalZ;

  uint32_t packedX = PackToSigned10Bit(normalX);
  uint32_t packedY = PackToSigned10Bit(normalY);
  uint32_t packedZ = PackToSigned10Bit(normalZ);

  uint32_t packedNormal =
      (packedX) | (packedY << 10) | (packedZ << 20); // NOLINT

  memcpy(dst, &packedNormal, sizeof(uint32_t)); // NOLINT
}

static void ConvertTangentToPacked10Bit(const uint8_t *__restrict src,
                                        uint8_t *__restrict dst) {
  float tangentX = 0.0F;
  float tangentY = 0.0F;
  float tangentZ = 0.0F;
  float tangentW = 0.0F;
  memcpy(&tangentX, src, sizeof(float));                       // NOLINT
  memcpy(&tangentY, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&tangentZ, src + (2 * sizeof(float)), sizeof(float)); // NOLINT
  memcpy(&tangentW, src + (3 * sizeof(float)), sizeof(float)); // NOLINT

  tangentZ = -tangentZ;

  uint32_t packedX = PackToSigned10Bit(tangentX);
  uint32_t packedY = PackToSigned10Bit(tangentY);
  uint32_t packedZ = PackToSigned10Bit(tangentZ);
  uint32_t packedW = tangentW > 0.0F ? 1 : 0; // Store sign in W

  uint32_t packedTangent =
      (packedX) | (packedY << 10) | (packedZ << 20) | (packedW << 30); // NOLINT

  memcpy(dst, &packedTangent, sizeof(uint32_t)); // NOLINT
}

static void ConvertPositionFlipZ(const uint8_t *__restrict src,
                                 uint8_t *__restrict dst) {
  float posX = 0.0F;
  float posY = 0.0F;
  float posZ = 0.0F;
  memcpy(&posX, src, sizeof(float));                       // NOLINT
  memcpy(&posY, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&posZ, src + (2 * sizeof(float)), sizeof(float)); // NOLINT

  posZ = -posZ;

  memcpy(dst, &posX, sizeof(float));                       // NOLINT
  memcpy(dst + sizeof(float), &posY, sizeof(float));       // NOLINT
  memcpy(dst + (2 * sizeof(float)), &posZ, sizeof(float)); // NOLINT
}

static auto PackToUnsigned8Bit(float value) -> uint {
  value = std::max(std::min(value, 1.0F), 0.0F);
  return static_cast<uint32_t>(std::round(value * 255.0F)); // NOLINT
}

static auto ConvertColorToPacked8Bit(const uint8_t *__restrict src,
                                     uint8_t *__restrict dst) -> void {
  float colorR = 0.0F;
  float colorG = 0.0F;
  float colorB = 0.0F;
  float colorA = 1.0F;
  memcpy(&colorR, src, sizeof(float));                       // NOLINT
  memcpy(&colorG, src + sizeof(float), sizeof(float));       // NOLINT
  memcpy(&colorB, src + (2 * sizeof(float)), sizeof(float)); // NOLINT
  memcpy(&colorA, src + (3 * sizeof(float)), sizeof(float)); // NOLINT

  uint32_t packedR = PackToUnsigned8Bit(colorR);
  uint32_t packedG = PackToUnsigned8Bit(colorG);
  uint32_t packedB = PackToUnsigned8Bit(colorB);
  uint32_t packedA = PackToUnsigned8Bit(colorA);

  uint32_t packedColor =
      (packedR) | (packedG << 8) | (packedB << 16) | (packedA << 24); // NOLINT

  memcpy(dst, &packedColor, sizeof(uint32_t));
}

// Converter functions; for example, float32vec3 to packed uint 10-bit per component.
// The key is the semantic, and the value is a function that takes the source data and writes the converted data to the destination buffer.
const std::unordered_map<std::string,
                         std::function<void(const uint8_t *src, uint8_t *dst)>>
    Converters = {{"NORMAL", ConvertNormalToPacked10Bit},
                  {"TANGENT", ConvertTangentToPacked10Bit},
                  {"POSITION", ConvertPositionFlipZ},
                  {"COLOR_0", ConvertColorToPacked8Bit},
                  {"TEXCOORD_0", nullptr}

};

inline auto TriangleTangent(Math::Vec3 vert0, Math::Vec3 vert1,
                            Math::Vec3 vert2, Math::Vec2 uv0, Math::Vec2 uv1,
                            Math::Vec2 uv2) -> Math::Vec4 {
  Math::Vec3 edge1 = vert1 - vert0;
  Math::Vec3 edge2 = vert2 - vert0;
  Math::Vec2 deltaUV1 = uv1 - uv0;
  Math::Vec2 deltaUV2 = uv2 - uv0;

  float factor = 1.0F / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

  Math::Vec4 tangent{};
  tangent.x = factor * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x); // NOLINT
  tangent.y = factor * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y); // NOLINT
  tangent.z = factor * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z); // NOLINT
  tangent.w = 1.0F;

  return tangent.Normalize();
}

static auto DecodeVertexIndex(const std::vector<uint8_t> &indices,
                              VkIndexType indexType, size_t linearIndex)
    -> size_t {
  const auto *indexBytes = indices.data();
  switch (indexType) {
  case VK_INDEX_TYPE_UINT8_EXT:
    return static_cast<size_t>(indexBytes[linearIndex]); // NOLINT
  case VK_INDEX_TYPE_UINT16: {
    uint16_t value = 0;
    memcpy(&value,
           indexBytes + (linearIndex * sizeof(uint16_t)), // NOLINT
           sizeof(uint16_t));
    return static_cast<size_t>(value);
  }
  case VK_INDEX_TYPE_UINT32: {
    uint32_t value = 0;
    memcpy(&value,
           indexBytes + (linearIndex * sizeof(uint32_t)), // NOLINT
           sizeof(uint32_t));
    return static_cast<size_t>(value);
  }
  default:
    return linearIndex;
  }
}

struct MikkUserData {
  uint8_t *vertexData;
  const uint8_t *indexData;
  size_t vertexStride;
  size_t vertexCount;
  size_t indexCount;
  size_t positionOffset;
  size_t normalOffset;
  size_t texcoordOffset;
  size_t tangentOffset;
  VkIndexType indexType;
};

static auto ResolveVertexIndex(const MikkUserData &data, int faceIndex,
                               int vertIndex) -> size_t {
  const size_t linearIndex =
      (static_cast<size_t>(faceIndex) * 3) + static_cast<size_t>(vertIndex);
  if (data.indexData == nullptr || data.indexCount == 0) {
    return linearIndex;
  }

  switch (data.indexType) {
  case VK_INDEX_TYPE_UINT8_EXT:
    return static_cast<size_t>(data.indexData[linearIndex]); // NOLINT
  case VK_INDEX_TYPE_UINT16: {
    uint16_t value = 0;
    memcpy(&value,
           data.indexData + (linearIndex * sizeof(uint16_t)), // NOLINT
           sizeof(uint16_t));
    return static_cast<size_t>(value);
  }
  case VK_INDEX_TYPE_UINT32: {
    uint32_t value = 0;
    memcpy(&value,
           data.indexData + (linearIndex * sizeof(uint32_t)), // NOLINT
           sizeof(uint32_t));
    return static_cast<size_t>(value);
  }
  default:
    return linearIndex;
  }
}

inline auto GenerateVertexTangents(const Graphics::VertexFormat &format,
                                   std::vector<uint8_t> &existingData,
                                   const std::vector<uint8_t> &indices,
                                   VkIndexType indexType) -> void {
  ZoneScoped;

  const auto *positionAttribute = format.GetAttribute("POSITION");
  const auto *normalAttribute = format.GetAttribute("NORMAL");
  const auto *texcoordAttribute = format.GetAttribute("TEXCOORD_0");
  const auto *tangentAttribute = format.GetAttribute("TANGENT");

  if (positionAttribute == nullptr || normalAttribute == nullptr ||
      texcoordAttribute == nullptr || tangentAttribute == nullptr) {
    return;
  }

  const size_t stride = format.GetStride(0);
  if (stride == 0 || existingData.empty()) {
    return;
  }

  const size_t vertexCount = existingData.size() / stride;
  if (vertexCount == 0) {
    return;
  }

  size_t indexCount = 0;
  if (!indices.empty()) {
    switch (indexType) {
    case VK_INDEX_TYPE_UINT8_EXT:
      indexCount = indices.size();
      break;
    case VK_INDEX_TYPE_UINT16:
      indexCount = indices.size() / sizeof(uint16_t);
      break;
    case VK_INDEX_TYPE_UINT32:
      indexCount = indices.size() / sizeof(uint32_t);
      break;
    default:
      indexCount = 0;
      break;
    }
  }

  MikkUserData userData{.vertexData = existingData.data(),
                        .indexData = indices.empty() ? nullptr : indices.data(),
                        .vertexStride = stride,
                        .vertexCount = vertexCount,
                        .indexCount = indexCount,
                        .positionOffset = positionAttribute->offset,
                        .normalOffset = normalAttribute->offset,
                        .texcoordOffset = texcoordAttribute->offset,
                        .tangentOffset = tangentAttribute->offset,
                        .indexType = indexType};

  SMikkTSpaceInterface mikkInterface{};
  SMikkTSpaceContext mikkContext{};

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-avoid-magic-numbers, cppcoreguidelines-pro-type-reinterpret-cast, hicpp-avoid-c-arrays, modernize-avoid-c-arrays)

  mikkInterface.m_getNumFaces = [](const SMikkTSpaceContext *pContext) -> int {
    auto *data = static_cast<MikkUserData *>(pContext->m_pUserData);
    if (data->indexCount > 0) {
      return static_cast<int>(data->indexCount / 3);
    }
    return static_cast<int>(data->vertexCount / 3);
  };

  mikkInterface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext *pContext,
                                            const int faceIndex) -> int {
    return 3; // Triangles only
  };

  mikkInterface.m_getPosition = [](const SMikkTSpaceContext *pContext,
                                   float outPos[], const int faceIndex,
                                   const int vertIndex) -> void {
    auto *data = static_cast<MikkUserData *>(pContext->m_pUserData);
    const size_t vertexIndex = ResolveVertexIndex(*data, faceIndex, vertIndex);

    const uint8_t *vertexPtr =
        data->vertexData + (vertexIndex * data->vertexStride);     // NOLINT
    const uint8_t *positionPtr = vertexPtr + data->positionOffset; // NOLINT

    memcpy(outPos, positionPtr, sizeof(float) * 3);
  };

  mikkInterface.m_getNormal = [](const SMikkTSpaceContext *pContext,
                                 float outNormal[], const int faceIndex,
                                 const int vertIndex) -> void {
    auto *data = static_cast<MikkUserData *>(pContext->m_pUserData);
    const size_t vertexIndex = ResolveVertexIndex(*data, faceIndex, vertIndex);

    const uint8_t *vertexPtr =
        data->vertexData + (vertexIndex * data->vertexStride); // NOLINT
    const uint8_t *normalPtr = vertexPtr + data->normalOffset; // NOLINT

    memcpy(outNormal, normalPtr, sizeof(float) * 3);
  };

  mikkInterface.m_getTexCoord = [](const SMikkTSpaceContext *pContext,
                                   float outUV[], const int faceIndex,
                                   const int vertIndex) -> void {
    auto *data = static_cast<MikkUserData *>(pContext->m_pUserData);
    const size_t vertexIndex = ResolveVertexIndex(*data, faceIndex, vertIndex);

    const uint8_t *vertexPtr =
        data->vertexData + (vertexIndex * data->vertexStride); // NOLINT
    const uint8_t *uvPtr = vertexPtr + data->texcoordOffset;   // NOLINT

    memcpy(outUV, uvPtr, sizeof(float) * 2);
  };

  mikkInterface.m_setTSpaceBasic =
      [](const SMikkTSpaceContext *pContext, const float tangent[],
         const float sign, const int faceIndex, const int vertIndex) -> void {
    auto *data = static_cast<MikkUserData *>(pContext->m_pUserData);
    const size_t vertexIndex = ResolveVertexIndex(*data, faceIndex, vertIndex);

    uint8_t *vertexPtr =
        data->vertexData + (vertexIndex * data->vertexStride); // NOLINT
    uint8_t *tangentPtr = vertexPtr + data->tangentOffset;     // NOLINT

    const std::array<float, 4> generated = {tangent[0], tangent[1], -tangent[2],
                                            sign};
    ConvertTangentToPacked10Bit(
        reinterpret_cast<const uint8_t *>(generated.data()), tangentPtr);
  };

  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic, cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-avoid-magic-numbers, cppcoreguidelines-pro-type-reinterpret-cast, hicpp-avoid-c-arrays, modernize-avoid-c-arrays)

  mikkContext.m_pInterface = &mikkInterface;
  mikkContext.m_pUserData = &userData;

  genTangSpaceDefault(&mikkContext);
}

// NOLINTNEXTLINE
inline auto FillVertexDataDefaults(const Graphics::VertexFormat &format,
                                   const fastgltf::Asset &asset,
                                   const fastgltf::Primitive &primitive,
                                   std::vector<uint8_t> &existingData,
                                   const std::vector<uint8_t> &indices,
                                   VkIndexType indexType) -> void {

  auto stride = format.GetStride(0);

  // Check for missing attributes and fill with defaults if needed.
  for (const auto &component : format.GetAttributes()) {
    bool hasAttribute = false;
    for (const auto &[semantic, accessorIndex] : primitive.attributes) {
      auto semanticView = std::string_view(semantic);
      if (semanticView == component.name) {
        hasAttribute = true;
        break;
      }
    }

    if (hasAttribute) {
      continue;
    }

    if (component.name == "COLOR_0") {
      // Fill missing vertex colors with white (1,1,1,1).
      uint32_t whiteColor = ~0U; // RGBA packed white
      for (size_t vertex = 0; vertex < existingData.size() / stride; ++vertex) {
        auto vert = existingData.data() + vertex * stride; // NOLINT
        memcpy(vert + component.offset, &whiteColor,       // NOLINT
               sizeof(uint32_t));
      }
    }

    if (component.name == "TANGENT") {
      // Generate tangents if missing.
      GenerateVertexTangents(format, existingData, indices, indexType);
    }
  }
}

inline auto // NOLINTNEXTLINE
LoadVertexData(const Graphics::VertexFormat &format,
               const fastgltf::Asset &asset,
               const fastgltf::Primitive &primitive,
               const std::vector<uint8_t> &indices, VkIndexType indexType)
    -> Result<std::vector<uint8_t>> {
  ZoneScoped;

  if (primitive.attributes.empty()) {
    return Error::Unexpected("Primitive has no attributes.");
  }

  // Determine vertex count from first accessor.
  const size_t vertexCount =
      asset.accessors.at(primitive.attributes.begin()->accessorIndex).count;

  // Compute output stride from the vertex format.
  const size_t outputStride = format.GetStride(0);

  std::vector<uint8_t> result(vertexCount * outputStride, 0);

  for (const auto &[semantic, accessorIndex] : primitive.attributes) {
    const fastgltf::Accessor &accessor = asset.accessors.at(accessorIndex);
    std::string_view semanticView(semantic);

    // Find matching vertex component in format.
    const Graphics::VertexComponent *attribute = nullptr;
    for (const auto &component : format.GetAttributes()) {
      if (component.name == semanticView) {
        attribute = &component;
        break;
      }
    }

    if (attribute == nullptr || attribute->format == VK_FORMAT_UNDEFINED) {
      // return Error::Unexpectedf("Couldn't match Format for: {}", semantic);
      continue; // Skip attributes that aren't in the vertex format.
    }

    if (accessor.count != vertexCount) {
      return Error::Unexpectedf(
          "Accessor count mismatch for {}: expected {}, got {}", semantic,
          vertexCount, accessor.count);
    }

    if (!accessor.bufferViewIndex.has_value()) {
      return Error::Unexpectedf("Accessor for {} has no bufferView.", semantic);
    }

    if (accessor.sparse.has_value()) {
      return Error::Unexpectedf("Sparse accessors not supported for {}.",
                                semantic);
    }

    const auto &bufferView =
        asset.bufferViews.at(accessor.bufferViewIndex.value());

    auto span = CHECK_RES(LoadBufferView(asset, bufferView));

    const auto srcComponentSize =
        fastgltf::getComponentByteSize(accessor.componentType);
    const auto componentCount =
        static_cast<size_t>(fastgltf::getNumComponents(accessor.type));
    const size_t srcElementSize = srcComponentSize * componentCount;
    auto vkFormatSize = Graphics::Format::GetSize(attribute->format);

    // byteStride of 0 means tightly packed.
    const size_t srcStride = bufferView.byteStride.value_or(srcElementSize);

    if (srcStride < srcElementSize) {
      return Error::Unexpectedf(
          "Buffer view stride too small for {}: need at least {}, got {}",
          semanticView, srcElementSize, srcStride);
    }

    // Bounds check: ensure all vertex data fits within the buffer.
    const size_t requiredSize =
        (vertexCount > 0 ? ((vertexCount - 1) * srcStride) + srcElementSize +
                               accessor.byteOffset
                         : 0);

    auto spansize = span.size();

    // Check if the accessor's data fits within the buffer view span.
    if (requiredSize > spansize) {
      return Error::Unexpectedf(
          "Buffer too small for attribute {}: need {}, have {}", semanticView,
          requiredSize, spansize);
    }

    const size_t dstElementSize = Graphics::Format::GetSize(attribute->format);
    const size_t dstOffset = attribute->offset;

    // Check if we need normalization (accessor.normalized + integer type).
    const bool needsNormalize =
        accessor.normalized &&
        (accessor.componentType == fastgltf::ComponentType::Byte ||
         accessor.componentType == fastgltf::ComponentType::UnsignedByte ||
         accessor.componentType == fastgltf::ComponentType::Short ||
         accessor.componentType == fastgltf::ComponentType::UnsignedShort);

    if (needsNormalize && srcElementSize != dstElementSize) {
      return Error::Unexpectedf(
          "Normalization requires matching src and dst element sizes for {}: "
          "got src {}, dst {}",
          semanticView, srcElementSize, dstElementSize);
    }

    auto resultSize = result.size();
    auto finalWriteSize =
        ((vertexCount - 1) * outputStride) + dstOffset + dstElementSize;
    if (finalWriteSize > resultSize) {
      return Error::Unexpectedf(
          "Output buffer too small for attribute {}: need {}, have {}",
          semanticView, finalWriteSize, resultSize);
    }

    auto converterIter = Converters.find(std::string(semantic));
    std::function<void(const uint8_t *src, uint8_t *dst)> converter = nullptr;
    if (converterIter != Converters.end()) {
      converter = converterIter->second;
    }

    auto *__restrict resultData = result.data();
    auto *__restrict spanData = span.data();

    for (size_t value = 0; value < vertexCount; ++value) {
      const uint8_t *srcPtr =
          spanData + value * srcStride + accessor.byteOffset;          // NOLINT
      uint8_t *dstPtr = resultData + value * outputStride + dstOffset; // NOLINT

      if (needsNormalize) {
        NormalizeAttribute(srcPtr, dstPtr, accessor.componentType,
                           componentCount);
      } else {
        if (converter != nullptr) {
          converter(srcPtr, dstPtr);
        } else {
          memcpy(dstPtr, srcPtr, srcElementSize);
        }
      }
    }
  }

  FillVertexDataDefaults(format, asset, primitive, result, indices, indexType);

  return result;
}

inline auto LoadIndexData(const fastgltf::Asset &asset,
                          const fastgltf::Primitive &primitive)
    -> Result<std::vector<uint8_t>> {
  if (!primitive.indicesAccessor.has_value()) {
    return std::vector<uint8_t>{};
  }

  const auto &accessor = asset.accessors.at(primitive.indicesAccessor.value());

  assert(accessor.bufferViewIndex.has_value());
  const auto &bufferView =
      asset.bufferViews.at(accessor.bufferViewIndex.value());

  auto span = CHECK_RES(LoadBufferView(asset, bufferView));

  auto componentSize = fastgltf::getComponentByteSize(accessor.componentType);
  auto finalOffset = accessor.byteOffset;
  auto finalLength = accessor.count * componentSize;

  // NOLINTNEXTLINE
  return std::vector<uint8_t>(span.data() + finalOffset,
                              span.data() + finalOffset + // NOLINT
                                  finalLength);
}

template <typename IndexT>
void OptimizeMesh(std::span<IndexT> indices, std::vector<uint8_t> &vertices,
                  size_t vertexCount, size_t vertexStride) {
  ZoneScoped;

  size_t indexCount = indices.size();

  thread_local std::vector<unsigned int> remap;
  remap.resize(vertexCount * vertexStride);

  size_t newVertexCount =
      meshopt_generateVertexRemap(remap.data(), indices.data(), indexCount,
                                  vertices.data(), vertexCount, vertexStride);

  meshopt_remapIndexBuffer(indices.data(), indices.data(), indexCount,
                           remap.data());

  meshopt_remapVertexBuffer(vertices.data(), vertices.data(), vertexCount,
                            vertexStride, remap.data());

  meshopt_optimizeVertexCache(indices.data(), indices.data(), indexCount,
                              newVertexCount);

  meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indexCount,
                              vertices.data(), newVertexCount, vertexStride);

  vertices.resize(newVertexCount * vertexStride);
}

auto GetInterleavedStride(const Graphics::VertexFormat &format) -> size_t {
  size_t stride = 0;
  for (int i = 0; i < format.GetBindingCount(); i++) {
    stride += format.GetStride(i);
  }
  return stride;
}

auto DeinterleaveVertexData(const std::vector<uint8_t> &interleavedData)
    -> std::vector<std::vector<uint8_t>> {
  ZoneScoped;

  struct VertexData {
    float position[3]; // NOLINT
    float texcoord[2]; // NOLINT
    uint32_t normal;
    uint32_t tangent;
    uint32_t color;
  };

  size_t vertexCount = interleavedData.size() / sizeof(VertexData);

  const auto *__restrict vertexArray = // NOLINTNEXTLINE
      reinterpret_cast<const VertexData *>(interleavedData.data());

  auto positionStride = sizeof(float) * 3;
  auto texcoordStride = sizeof(float) * 2;
  auto normalTangentStride = sizeof(uint32_t) * 2;
  auto colorStride = sizeof(uint32_t);

  auto positionData = std::vector<uint8_t>(vertexCount * positionStride);
  auto texcoordData = std::vector<uint8_t>(vertexCount * texcoordStride);
  auto normalTangentData =
      std::vector<uint8_t>(vertexCount * normalTangentStride);
  auto colorData = std::vector<uint8_t>(vertexCount * colorStride);

  auto *__restrict positionPtr = positionData.data();
  auto *__restrict texcoordPtr = texcoordData.data();
  auto *__restrict normalTangentPtr = normalTangentData.data();
  auto *__restrict colorPtr = colorData.data();

  for (int vertex = 0; vertex < vertexCount; ++vertex) {
    const auto &srcVertex = vertexArray[vertex]; // NOLINT

    // NOLINTNEXTLINE
    memcpy(positionPtr + (vertex * positionStride), &srcVertex.position,
           positionStride);
    // NOLINTNEXTLINE
    memcpy(texcoordPtr + (vertex * texcoordStride), &srcVertex.texcoord,
           texcoordStride);
    // NOLINTNEXTLINE
    memcpy(normalTangentPtr + (vertex * normalTangentStride), &srcVertex.normal,
           normalTangentStride);
    // NOLINTNEXTLINE
    memcpy(colorPtr + (vertex * colorStride), &srcVertex.color, colorStride);
  }

  return {positionData, texcoordData, normalTangentData, colorData};
}

using Comp = Graphics::VertexComponent;

static const std::vector<Comp> InitialVertexComponents = {
    Comp{.name = "POSITION",
         .location = 0,
         .binding = 0,
         .format = VK_FORMAT_R32G32B32_SFLOAT},
    Comp{.name = "TEXCOORD_0",
         .location = 1,
         .binding = 0,
         .format = VK_FORMAT_R32G32_SFLOAT},
    Comp{.name = "NORMAL",
         .location = 2,
         .binding = 0,
         .format = VK_FORMAT_R32_UINT},
    Comp{.name = "TANGENT",
         .location = 3,
         .binding = 0,
         .format = VK_FORMAT_R32_UINT},
    Comp{.name = "COLOR_0",
         .location = 4,
         .binding = 0,
         .format = VK_FORMAT_R8G8B8A8_UNORM}};

struct VertexData {
  float position[3]; // NOLINT
  float texcoord[2]; // NOLINT
  uint32_t normal;
  uint32_t tangent;
  uint32_t color;
};

static const Graphics::VertexFormat SeparateVertexFormat = [] -> auto {
  std::vector<Comp> comps = InitialVertexComponents;
  for (auto comp : comps) {
    comp.location = 0;
  }

  comps.at(0).binding = 0; // POSITION
  comps.at(1).binding = 1; // TEXCOORD_0
  comps.at(2).binding = 2; // NORMAL + TANGENT
  comps.at(3).binding = 2; // NORMAL + TANGENT
  comps.at(4).binding = 3; // COLOR_0

  return Graphics::VertexFormat(comps);
}();

static const Graphics::VertexFormat InitialVertexFormat = [] -> auto {
  return Graphics::VertexFormat(InitialVertexComponents);
}();

struct MeshLoadInfo {
  std::vector<uint8_t> Vertices;
  std::vector<uint8_t> Indices;

  VkIndexType IndexType;
  std::string Name;
  flecs::entity Parent;
  fastgltf::OptionalWithFlagValue<unsigned long> MaterialIndex;

  // Only set after calling OptimiseMesh
  std::vector<std::vector<uint8_t>> DeinterleavedData;
};

inline auto OptimiseMesh(MeshLoadInfo &info) {
  ZoneScoped;

  auto stride = InitialVertexFormat.GetStride(0);
  auto vertexCount = info.Vertices.size() / stride;

  switch (info.IndexType) {
  case VK_INDEX_TYPE_UINT8:
    OptimizeMesh( // NOLINTNEXTLINE
        std::span(reinterpret_cast<uint8_t *>(info.Indices.data()),
                  info.Indices.size() / sizeof(uint8_t)),
        info.Vertices, vertexCount, stride);
    break;
  case VK_INDEX_TYPE_UINT16:
    OptimizeMesh( // NOLINTNEXTLINE
        std::span(reinterpret_cast<uint16_t *>(info.Indices.data()),
                  info.Indices.size() / sizeof(uint16_t)),
        info.Vertices, vertexCount, stride);
    break;
  case VK_INDEX_TYPE_UINT32:
    OptimizeMesh( // NOLINTNEXTLINE
        std::span(reinterpret_cast<uint32_t *>(info.Indices.data()),
                  info.Indices.size() / sizeof(uint32_t)),
        info.Vertices, vertexCount, stride);
    break;
  case VK_INDEX_TYPE_NONE_KHR:
  case VK_INDEX_TYPE_MAX_ENUM:
    break;
  }

  info.DeinterleavedData = DeinterleaveVertexData(info.Vertices);
}

inline auto LoadMesh(const Graphics::GraphicsContext &context,
                     flecs::world *world, MeshLoadInfo &info,
                     const fastgltf::Asset &asset,
                     const std::string_view &basePath) -> Error {
  ZoneScoped;

  std::vector<std::span<const uint8_t>> deinterleavedSpans;
  deinterleavedSpans.reserve(info.DeinterleavedData.size());
  for (const auto &data : info.DeinterleavedData) {
    deinterleavedSpans.emplace_back(data.data(), data.size());
  }

  Graphics::MeshCreationInfo meshCreationInfo{
      .vertexFormat = &SeparateVertexFormat,
      .vertexData = deinterleavedSpans,
      .debugName = std::string(info.Name),
  };

  auto mesh = CHECK_RES(Graphics::Mesh::Create(context, meshCreationInfo));

  if (!info.Indices.empty()) {
    CHECK_ERR(mesh->SetIndices(context, info.Indices, info.IndexType));
  }

  CHECK_ERR(mesh->CreateBLAS(context));

  auto geometry = world->entity(GetUniqueName("Geometry").c_str());
  geometry.set<Engine::Geometry>(Engine::Geometry{.mesh = mesh});
  geometry.add<Engine::Transform>();

  Engine::LocalBounds localBounds{};

  std::span<const VertexData> vertexDataSpan(
      reinterpret_cast<const VertexData *>(info.Vertices.data()), // NOLINT
      info.Vertices.size() / sizeof(VertexData));

  for (const auto &vertex : vertexDataSpan) {
    Math::Vec3 pos(vertex.position[0], vertex.position[1], vertex.position[2]);
    localBounds.Bounds.Grow(pos);
  }

  geometry.set<Engine::LocalBounds>(localBounds);
  geometry.add<Engine::WorldBounds>();
  geometry.child_of(info.Parent);

  // Load material if present.
  if (info.MaterialIndex.has_value()) {
    const auto &material = asset.materials[info.MaterialIndex.value()];
    geometry.add<Engine::Renderer::Material>();
    auto rendererMaterial = Ref<Engine::Renderer::LuaMaterial>::Make(geometry);

    CHECK_ERR(
        LoadMaterial(context, asset, basePath, material, rendererMaterial));
  }

  return {};
}

struct MeshVertexDataLoadInfo {
  fastgltf::Asset const *Asset;
  fastgltf::Primitive const *Primitive;
};

inline auto LoadMeshVertexData(MeshVertexDataLoadInfo &info,
                               MeshLoadInfo &output) -> Error {
  output.Indices = CHECK_RES(LoadIndexData(*info.Asset, *info.Primitive));
  output.IndexType = VK_INDEX_TYPE_MAX_ENUM;
  if (!output.Indices.empty()) {
    const auto &accessor =
        info.Asset->accessors.at(info.Primitive->indicesAccessor.value());
    switch (accessor.componentType) {
    case fastgltf::ComponentType::UnsignedByte:
      output.IndexType = VK_INDEX_TYPE_UINT8_EXT;
      break;
    case fastgltf::ComponentType::UnsignedShort:
      output.IndexType = VK_INDEX_TYPE_UINT16;
      break;
    case fastgltf::ComponentType::UnsignedInt:
      output.IndexType = VK_INDEX_TYPE_UINT32;
      break;
    default:
      return Error::Create("Unsupported index component type.");
    }
  }

  output.Vertices = CHECK_RES(LoadVertexData(InitialVertexFormat, *info.Asset,
                                             *info.Primitive, output.Indices,
                                             output.IndexType));

  return {};
}

inline auto // NOLINTNEXTLINE
LoadNode(
    flecs::world *world, Graphics::GraphicsContext &context,
    const fastgltf::Asset &asset, const std::string_view &basePath,
    const fastgltf::Node &gltfNode, std::unordered_set<size_t> &parentIndices,
    std::vector<std::pair<MeshVertexDataLoadInfo, MeshLoadInfo>> &loadInfos)
    -> Result<std::vector<flecs::entity>> {
  bool isMesh = gltfNode.meshIndex.has_value();
  bool isSkin = gltfNode.skinIndex.has_value();
  bool isCamera = gltfNode.cameraIndex.has_value();
  bool isLight = gltfNode.lightIndex.has_value();

  ZoneScoped;

  // Note: checking if children aren't empty might cull some nodes, should check if this is an issue.
  bool isNode = !isMesh && !isSkin && !isCamera && !isLight;

  Engine::Transform transform{};

  if (std::holds_alternative<fastgltf::TRS>(gltfNode.transform)) {
    const auto &trs = std::get<fastgltf::TRS>(gltfNode.transform);

    transform.SetPosition(trs.translation[0], trs.translation[1],
                          -trs.translation[2]);
    // Basis conversion with Z-axis reflection: q' = (-x, -y, z, w)
    transform.SetRotation(-trs.rotation[0], -trs.rotation[1], trs.rotation[2],
                          trs.rotation[3]);
    transform.SetScale(trs.scale[0], trs.scale[1], trs.scale[2]);
  } else {
    // Shouldn't be hit. Since we specified DecomposeNodeMatrices, all matrices should
    // have been decomposed.
    return Error::Unexpected("Attempted to load a node with matrix transform.");
  }

  if (isNode) {
    auto node = world->entity(gltfNode.name.c_str());
    node.add<Engine::Node>();
    node.add<Engine::Transform>();
    node.add<Engine::Userdata>();
    node.set<Engine::Transform>(transform);
    node.add<Engine::WorldBounds>();

    for (const auto &childIndex : gltfNode.children) {
      if (parentIndices.contains(childIndex)) {
        return Error::Unexpectedf(
            "Detected cyclic reference in node hierarchy at index {}.",
            childIndex);
      }

      const auto &childGltfNode = asset.nodes[childIndex];

      parentIndices.emplace(childIndex);

      auto childNode =
          CHECK_RES(LoadNode(world, context, asset, basePath, childGltfNode,
                             parentIndices, loadInfos));

      for (auto &childObject : childNode) {
        childObject.child_of(node);
      }
    }

    return std::vector<flecs::entity>{node};
  }

  if (isMesh) {
    auto meshIndex = *gltfNode.meshIndex;
    const auto &gltfMesh = asset.meshes[meshIndex];

    std::vector<flecs::entity> shapes;

    for (const auto &primitive : gltfMesh.primitives) {
      // Each primitive corresponds to a separate mesh in our engine.
      auto shapeEntity =
          world->entity(GetUniqueName(gltfMesh.name.c_str()).c_str());
      shapeEntity.add<Engine::Shape>();
      shapeEntity.add<Engine::WorldBounds>();
      // shapeEntity.add<Engine::LocalBounds>(); No local bounds, only based on child geometry bounds.
      shapeEntity.set<Engine::Transform>(transform);

      shapes.emplace_back(shapeEntity);

      auto lod = world->entity(GetUniqueName("LOD").c_str());

      MeshLoadInfo loadInfo = {
          .Name = std::string(gltfMesh.name),
          .Parent = lod,
          .MaterialIndex = primitive.materialIndex,
      };

      MeshVertexDataLoadInfo verticesLoadInfo{
          .Asset = &asset,
          .Primitive = &primitive,
      };

      loadInfos.emplace_back(verticesLoadInfo, loadInfo);

      lod.add<Engine::LevelOfDetail>();
      lod.add<Engine::WorldBounds>();
      lod.add<Engine::Transform>();

      lod.child_of(shapeEntity);
    }

    return shapes;
  }

  if (isSkin) {
    return {};
  }

  if (isCamera) {
    return {};
  }

  if (isLight) {
    return {};
  }

  return Error::Unexpected("Failed to determine node type.");
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path,
                   flecs::world *world) -> Error {
  ZoneScoped;
  /// Load the file into a data buffer.

  auto bytes = CHECK_RES(Filesystem::ReadFile(path));
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto *bytedata = reinterpret_cast<const std::byte *>(bytes.data());

  /// Create a glTF data buffer from the loaded file data.
  auto data = fastgltf::GltfDataBuffer::FromBytes(bytedata, bytes.size());
  if (data.error() != fastgltf::Error::None) {
    // The file couldn't be loaded, or the buffer could not be allocated.
    return Error::Create("Failed to load glTF data buffer.");
  }

  auto asset =
      Parser.loadGltf(data.get(), Path::Directory(path),
                      fastgltf::Options::DecomposeNodeMatrices |
                          fastgltf::Options::DontRequireValidAssetMember);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    // Some error occurred while reading the buffer, parsing the JSON, or validating the data.
    return Error::Create(fastgltf::getErrorName(error));
  }

  /// Validate the loaded asset.
  auto validationError = fastgltf::validate(asset.get());

  if (validationError != fastgltf::Error::None) {
    return Error::Create("glTF asset validation failed: " +
                         std::string(fastgltf::getErrorName(validationError)));
  }

  Buffers.reserve(asset->buffers.size());
  const auto &basePath = Path::Directory(path);
  const auto view = std::string_view(basePath);

  for (size_t i = 0; i < asset->buffers.size(); ++i) {
    const auto &buffer = asset->buffers[i];
    auto bufferData = CHECK_RES(LoadDataSource(asset.get(), view, buffer.data));
    Buffers.emplace_back(bufferData.begin(), bufferData.end());
  }

  std::vector<size_t> indices(asset->images.size());
  std::ranges::iota(indices, 0);
  ImageCache.resize(asset->images.size());
  Error imageLoadError = Error::Success();

  std::for_each(std::execution::par, indices.begin(), indices.end(),
                [&](const auto &index) -> auto {
                  const fastgltf::Image &image = asset->images[index];
                  auto loadResult =
                      LoadDataSource(asset.get(), basePath, image.data);
                  if (Error::IsError(loadResult)) {
                    imageLoadError = loadResult.error();
                    return;
                  }

                  auto span = loadResult.value();

                  if (Image::IsDDS(span)) {
                    auto imageDataResult =
                        Image::CompressedImageData::Create(span);
                    if (Error::IsError(imageDataResult)) {
                      imageLoadError = imageDataResult.error();
                      return;
                    }
                    ImageCache.at(index) = imageDataResult.value();
                  } else {
                    auto imageDataResult = Image::ImageData::Create(span);
                    if (Error::IsError(imageDataResult)) {
                      imageLoadError = imageDataResult.error();
                      return;
                    }

                    ImageCache.at(index) = imageDataResult.value();
                  }
                });

  if (Error::IsError(imageLoadError)) {
    return imageLoadError;
  }

  std::unordered_set<size_t> parentIndices;
  std::vector<std::pair<MeshVertexDataLoadInfo, MeshLoadInfo>> loadInfos;

  // loop through scenes
  for (const auto &glTFScene : asset->scenes) {
    for (const auto &nodeIndex : glTFScene.nodeIndices) {
      const auto &gltfNode = asset->nodes[nodeIndex];

      parentIndices.emplace(nodeIndex);

      CHECK_RES(LoadNode(world, context, asset.get(), view, gltfNode,
                         parentIndices, loadInfos));
    }
  }

  std::vector<size_t> indices2(loadInfos.size());
  std::ranges::iota(indices2, 0);
  auto loadError = Error::Success();

  std::for_each(std::execution::par, indices2.begin(), indices2.end(),
                [&](const auto &index) -> auto {
                  std::pair<MeshVertexDataLoadInfo, MeshLoadInfo> &infoPair =
                      loadInfos.at(index);

                  auto error =
                      LoadMeshVertexData(infoPair.first, infoPair.second);
                  if (error.IsError()) {
                    loadError = error;
                  }

                  OptimiseMesh(infoPair.second);
                });

  CHECK_ERR(loadError);

  for (auto &info : loadInfos) {
    CHECK_ERR(LoadMesh(context, world, info.second, asset.get(), view));
  }

  {
    std::lock_guard<std::mutex> lock(URICacheMutex);

    Buffers.clear();
    URICache.clear();
    ImageCache.clear();
    TextureCache.clear();
  }

  return Error::Success();
}

} // namespace glTF