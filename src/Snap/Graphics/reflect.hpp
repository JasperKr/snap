#pragma once

#include "Graphics/bufferformat.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/error.hpp"
#include "Modules/stackVector.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Graphics::Reflect {

enum class ScalarType : uint8_t {
  Unknown,
  Float,
  Int,
  UInt,
  Bool,
};

inline auto FromScalarType(slang::TypeReflection::ScalarType baseType)
    -> ScalarType {
  switch (baseType) {
  case slang::TypeReflection::ScalarType::Float32:
    return ScalarType::Float;
  case slang::TypeReflection::ScalarType::Int32:
    return ScalarType::Int;
  case slang::TypeReflection::ScalarType::UInt32:
    return ScalarType::UInt;
  case slang::TypeReflection::ScalarType::Bool:
    return ScalarType::Bool;
  default:
    return ScalarType::Unknown;
  }
}

// Should be combined with ScalarType to form a complete type
enum class VectorType : uint8_t {
  Unknown,
  Vector2,
  Vector3,
  Vector4,
};

inline auto ToVectorType(uint8_t elementCount) -> VectorType {
  if (elementCount == 2) {
    return VectorType::Vector2;
  }
  if (elementCount == 3) {
    return VectorType::Vector3;
  }
  if (elementCount == 4) {
    return VectorType::Vector4;
  }

  return VectorType::Unknown;
}

inline auto VectorChannelCount(VectorType vectorType) -> uint8_t {
  switch (vectorType) {
  case VectorType::Vector2:
    return 2;
  case VectorType::Vector3:
    return 3;
  case VectorType::Vector4:
    return 4;
  default:
    return 0;
  }
}

enum class MatrixType : uint8_t {
  Unknown,
  Matrix2x2,
  Matrix3x3,
  Matrix4x4,
  Matrix2x3,
  Matrix2x4,
  Matrix3x2,
  Matrix3x4,
  Matrix4x2,
  Matrix4x3,
};

inline auto MatrixDimensions(MatrixType matrixType)
    -> std::pair<uint8_t, uint8_t> {
  switch (matrixType) {
  case MatrixType::Matrix2x2:
    return {2, 2};
  case MatrixType::Matrix3x3:
    return {3, 3};
  case MatrixType::Matrix4x4:
    return {4, 4};
  case MatrixType::Matrix2x3:
    return {2, 3};
  case MatrixType::Matrix2x4:
    return {2, 4};
  case MatrixType::Matrix3x2:
    return {3, 2};
  case MatrixType::Matrix3x4:
    return {3, 4};
  case MatrixType::Matrix4x2:
    return {4, 2};
  case MatrixType::Matrix4x3:
    return {4, 3};
  default:
    return {0, 0};
  }
}

inline auto ToMatrixType(uint8_t rowCount, uint8_t columnCount) -> MatrixType {
  if (rowCount == 2 && columnCount == 2) {
    return MatrixType::Matrix2x2;
  }
  if (rowCount == 3 && columnCount == 3) {
    return MatrixType::Matrix3x3;
  }
  if (rowCount == 4 && columnCount == 4) {
    return MatrixType::Matrix4x4;
  }
  if (rowCount == 2 && columnCount == 3) {
    return MatrixType::Matrix2x3;
  }
  if (rowCount == 2 && columnCount == 4) {
    return MatrixType::Matrix2x4;
  }
  if (rowCount == 3 && columnCount == 2) {
    return MatrixType::Matrix3x2;
  }
  if (rowCount == 3 && columnCount == 4) {
    return MatrixType::Matrix3x4;
  }
  if (rowCount == 4 && columnCount == 2) {
    return MatrixType::Matrix4x2;
  }
  if (rowCount == 4 && columnCount == 3) {
    return MatrixType::Matrix4x3;
  }

  return MatrixType::Unknown;
}

static inline auto ResourceKeyToString(const Graphics::ResourceKey &key)
    -> std::string {
  std::string result;
  for (const auto &part : key) {
    if (!result.empty()) {
      result += ".";
    }
    result += part.ToString();
  }
  return result;
}

struct ResourceBase {
  // Element size in bytes, not accounting for array size but accounting for alignment
  uint32_t size;

  // Offset in parent structure, if applicable
  uint32_t offset;

  // yeah
  uint32_t arraySize;
};

struct SamplerInfo : ResourceBase {
  uint32_t set;
  uint32_t binding;

  SlangResourceShape shape;
  SlangResourceAccess access;

  VkAccessFlags2 accessFlags;

  VkFormat format;
};

struct AccelerationStructureInfo : ResourceBase {
  uint32_t set;
  uint32_t binding;

  SlangResourceShape shape;
  SlangResourceAccess access;

  VkAccessFlags2 accessFlags;
};

struct ScalarInfo : ResourceBase {
  ScalarType type;
};

struct VectorInfo : ResourceBase {
  ScalarType scalarType;
  VectorType vectorType;
};

struct MatrixInfo : ResourceBase {
  MatrixType matrixType;
};

enum class StructFieldVariant : uint8_t {
  Unknown,
  Scalar,
  Vector,
  Matrix,
  Struct,
};

struct ResourceInfo;

struct StructInfo : ResourceBase {
  const char *name;
  std::vector<ResourceInfo> fields;
  std::vector<uint32_t> fieldOffsets;

  uint32_t alignment;
};

enum class BufferType : uint8_t {
  Unknown,
  Uniform,
  Storage,
  PushConstant,
};

struct BufferInfo : ResourceBase {
  const char *name;

  uint32_t set;
  uint32_t binding;

  SlangResourceAccess access;
  VkAccessFlags2 accessFlags;
  BufferType bufferType;

  [[nodiscard]] constexpr auto IsStruct() const -> bool {
    return std::holds_alternative<StructInfo>(info);
  }
  [[nodiscard]] constexpr auto IsScalar() const -> bool {
    return std::holds_alternative<ScalarInfo>(info);
  }
  [[nodiscard]] constexpr auto IsVector() const -> bool {
    return std::holds_alternative<VectorInfo>(info);
  }
  [[nodiscard]] constexpr auto IsMatrix() const -> bool {
    return std::holds_alternative<MatrixInfo>(info);
  }

  template <typename T>
  [[nodiscard]] constexpr auto GetInfo() const -> const T & {
    return std::get<T>(info);
  }
  template <typename T> [[nodiscard]] constexpr auto GetInfo() -> T & {
    return std::get<T>(info);
  }

  template <typename T> [[nodiscard]] constexpr auto Is() const -> bool {
    return std::holds_alternative<T>(info);
  }

  std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo> info;

  [[nodiscard]] auto ToString() const -> std::string;
};

using ResourceVariant =
    std::variant<SamplerInfo, ScalarInfo, VectorInfo, MatrixInfo, BufferInfo,
                 StructInfo, AccelerationStructureInfo>;

struct ResourceInfo {
  ResourceInfo() = default;
  explicit ResourceInfo(const char *name) : name(name) {}
  ResourceInfo(const char *name, ResourceVariant info)
      : name(name), info(std::move(info)) {}
  ResourceInfo(const char *name, VkShaderStageFlags stages,
               ResourceVariant info)
      : name(name), stages(stages), info(std::move(info)) {}
  ResourceInfo(const ResourceInfo &other) = default;
  ResourceInfo(ResourceInfo &&other) noexcept
      : name(other.name), stages(other.stages), info(std::move(other.info)),
        offset(other.offset), set(other.set), binding(other.binding) {}
  auto operator=(const ResourceInfo &other) -> ResourceInfo & {
    if (this != &other) {
      name = other.name;
      stages = other.stages;
      info = other.info;
    }
    return *this;
  }
  auto operator=(ResourceInfo &&other) noexcept -> ResourceInfo & {
    if (this != &other) {
      name = other.name;
      stages = other.stages;
      info = std::move(other.info);
    }
    return *this;
  }
  ~ResourceInfo() = default;

  const char *name{};
  VkShaderStageFlags stages = VK_SHADER_STAGE_ALL;
  uint32_t offset{};
  uint32_t set{};
  uint32_t binding{};

  [[nodiscard]] constexpr auto IsBuffer() const -> bool {
    return std::holds_alternative<BufferInfo>(info);
  }
  [[nodiscard]] constexpr auto IsSampler() const -> bool {
    return std::holds_alternative<SamplerInfo>(info);
  }
  [[nodiscard]] constexpr auto IsStruct() const -> bool {
    return std::holds_alternative<StructInfo>(info);
  }
  [[nodiscard]] constexpr auto IsScalar() const -> bool {
    return std::holds_alternative<ScalarInfo>(info);
  }
  [[nodiscard]] constexpr auto IsVector() const -> bool {
    return std::holds_alternative<VectorInfo>(info);
  }
  [[nodiscard]] constexpr auto IsMatrix() const -> bool {
    return std::holds_alternative<MatrixInfo>(info);
  }
  [[nodiscard]] constexpr auto IsAccelerationStructure() const -> bool {
    return std::holds_alternative<AccelerationStructureInfo>(info);
  }
  template <typename T>
  [[nodiscard]] constexpr auto GetInfo() const -> const T & {
    return std::get<T>(info);
  }
  template <typename T>
  [[nodiscard]] constexpr auto GetInfoPtr() const -> const T * {
    if (std::holds_alternative<T>(info)) {
      return &std::get<T>(info);
    }
    return nullptr;
  }
  template <typename T> [[nodiscard]] constexpr auto Is() const -> bool {
    return std::holds_alternative<T>(info);
  }
  [[nodiscard]] auto GetOffset() const -> uint32_t {
    return std::visit([](const auto &data) -> uint32_t { return data.offset; },
                      info);
  }
  [[nodiscard]] auto GetArraySize() const -> uint32_t {
    return std::visit(
        [](const auto &data) -> uint32_t { return data.arraySize; }, info);
  }
  [[nodiscard]] constexpr auto GetSize() const -> uint32_t {
    return std::visit([](const auto &data) -> uint32_t { return data.size; },
                      info);
  }

  ResourceVariant info;

  [[nodiscard]] auto GetTypename() const -> std::string_view {
    if (IsBuffer()) {
      return "Buffer";
    }
    if (IsSampler()) {
      return "Sampler";
    }
    if (IsStruct()) {
      return "Struct";
    }
    if (IsScalar()) {
      return "Scalar";
    }
    if (IsVector()) {
      return "Vector";
    }
    if (IsMatrix()) {
      return "Matrix";
    }
    if (IsAccelerationStructure()) {
      return "AccelerationStructure";
    }
    return "Unknown";
  }

  [[nodiscard]] auto ToString() const -> std::string {
    auto result = std::format(
        "Resource Name: {} Type: {} Offset: {}, Size: {}, Array Size: {}\n",
        name, GetTypename(), GetOffset(), GetSize(), GetArraySize());

    if (IsBuffer()) {
      const auto &bufferInfo = std::get<BufferInfo>(info);
      result += "  Buffer Type: ";
      switch (bufferInfo.bufferType) {
      case BufferType::Uniform:
        result += "Uniform\n";
        break;
      case BufferType::Storage:
        result += "Storage\n";
        break;
      case BufferType::PushConstant:
        result += "Push Constant\n";
        break;
      default:
        result += "Unknown\n";
        break;
      }
      result += "  Set: " + std::to_string(bufferInfo.set) + "\n";
      result += "  Binding: " + std::to_string(bufferInfo.binding) + "\n";
    } else if (IsSampler()) {
      const auto &samplerInfo = std::get<SamplerInfo>(info);
      result += "  Set: " + std::to_string(samplerInfo.set) + "\n";
      result += "  Binding: " + std::to_string(samplerInfo.binding) + "\n";
    } else if (IsAccelerationStructure()) {
      const auto &asInfo = std::get<AccelerationStructureInfo>(info);
      result += "  Set: " + std::to_string(asInfo.set) + "\n";
      result += "  Binding: " + std::to_string(asInfo.binding) + "\n";
    } else if (IsStruct()) {
      const auto &structInfo = std::get<StructInfo>(info);
      result += "  Fields:\n";
      for (const auto &field : structInfo.fields) {
        result += "    - " + field.ToString();
      }
    }
    return result;
  }
};

constexpr auto
SlangResourceAccessToVkAccessFlags_Buffer(SlangResourceAccess access)
    -> VkAccessFlags2 {
  switch (access) {
  case SLANG_RESOURCE_ACCESS_READ:
    return VK_ACCESS_2_SHADER_READ_BIT;
  case SLANG_RESOURCE_ACCESS_READ_WRITE:
    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  case SLANG_RESOURCE_ACCESS_WRITE:
    return VK_ACCESS_2_SHADER_WRITE_BIT;
  default:
    return VK_ACCESS_2_SHADER_READ_BIT;
  }
}

constexpr auto
SlangResourceAccessToVkAccessFlags_Sampler(SlangResourceAccess access)
    -> VkAccessFlags2 {
  switch (access) {
  case SLANG_RESOURCE_ACCESS_NONE:
  case SLANG_RESOURCE_ACCESS_READ:
    return VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  case SLANG_RESOURCE_ACCESS_READ_WRITE:
    return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  case SLANG_RESOURCE_ACCESS_WRITE:
    return VK_ACCESS_2_SHADER_WRITE_BIT;
  default:
    return 0;
  }
}

auto ResourceInfoToBufferFormat(const ResourceInfo &info, Standard std)
    -> Result<std::variant<Graphics::BufferFormat, Graphics::BufferComponent>>;

struct FlattenedReflection {
  std::unordered_map<ResourceKey, ResourceInfo, ResourceKeyHash> keyToInfo;
  std::unordered_map<uint32_t, ResourceInfo> bindingToInfo;

  uint32_t size{}; // Not stride
};

struct ShaderReflection {
  std::vector<ResourceInfo> resources;
  std::unordered_map<uint64_t, ResourceInfo> slotToInfo;
  BufferInfo globals;
  Graphics::BufferFormat globalBufferFormat;
  bool hasGlobals{false};

  // NOLINTNEXTLINE
  auto ConstructUBOStruct(uint32_t set, uint32_t binding) -> Error;
  auto FlattenReflection() -> Error;

  // Set -> FlattenedReflection
  std::unordered_map<uint32_t, FlattenedReflection> flattened;
  std::unordered_map<ResourceKey, uint64_t, ResourceKeyHash> keyToSlot;
  std::vector<FlattenedReflection> pushBuffers;

  using SlotsStorage =
      std::unordered_map<uint32_t, Math::StackVector<uint64_t, 16>>; // NOLINT

  // All buffers and textures corresponding to a given set, for quick lookup when binding descriptors
  SlotsStorage bufferSlotsBySet;
  SlotsStorage textureSlotsBySet;
  SlotsStorage accelerationStructureSlotsBySet;
};

auto ReflectShader(const Graphics::GraphicsContext &context,
                   slang::ProgramLayout *programLayout,
                   ShaderReflection &outReflection) -> Error;

} // namespace Graphics::Reflect