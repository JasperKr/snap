#pragma once

#include "Graphics/bufferformat.hpp"
#include "Graphics/descriptorSlots.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/error.hpp"
#include "Modules/stackVector.hpp"
#include "slang/slang.h"
#include <cstdint>
#include <memory>
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

auto FromScalarType(slang::TypeReflection::ScalarType baseType) -> ScalarType;

// Should be combined with ScalarType to form a complete type
enum class VectorType : uint8_t {
  Unknown,
  Vector2,
  Vector3,
  Vector4,
};

auto ToVectorType(uint8_t elementCount) -> VectorType;

auto VectorChannelCount(VectorType vectorType) -> uint8_t;

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

auto MatrixDimensions(MatrixType matrixType) -> std::pair<uint8_t, uint8_t>;
auto ToMatrixType(uint8_t rowCount, uint8_t columnCount) -> MatrixType;
auto ResourceKeyToString(const Graphics::ResourceKey &key) -> std::string;

struct ResourceBase {
  // Element size in bytes, not accounting for array size but accounting for alignment
  uint32_t size;

  // Offset in parent structure, if applicable
  uint32_t offset;
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

  [[nodiscard]] constexpr auto IsStruct() const -> bool;
  [[nodiscard]] constexpr auto IsScalar() const -> bool;
  [[nodiscard]] constexpr auto IsVector() const -> bool;
  [[nodiscard]] constexpr auto IsMatrix() const -> bool;

  template <typename T> [[nodiscard]] constexpr auto GetInfo() const -> const T & { return std::get<T>(info); }
  template <typename T> [[nodiscard]] constexpr auto GetInfo() -> T & { return std::get<T>(info); }

  template <typename T> [[nodiscard]] constexpr auto Is() const -> bool { return std::holds_alternative<T>(info); }

  std::shared_ptr<ResourceInfo> info;

  [[nodiscard]] auto ToString() const -> std::string;
};

using ResourceVariant = std::variant<ScalarInfo, VectorInfo, MatrixInfo, StructInfo>;

struct ResourceInfo {
  ResourceInfo() = default;
  explicit ResourceInfo(const char *name) : name(name) {}
  ResourceInfo(const char *name, ResourceVariant info) : name(name), info(std::move(info)) {}
  ResourceInfo(const char *name, VkShaderStageFlags stages, ResourceVariant info) : name(name), stages(stages), info(std::move(info)) {}

  const char *name{};
  VkShaderStageFlags stages = VK_SHADER_STAGE_ALL;
  uint32_t offset{};
  uint32_t arraySize = 1;

  [[nodiscard]] constexpr auto IsStruct() const -> bool { return std::holds_alternative<StructInfo>(info); }
  [[nodiscard]] constexpr auto IsScalar() const -> bool { return std::holds_alternative<ScalarInfo>(info); }
  [[nodiscard]] constexpr auto IsVector() const -> bool { return std::holds_alternative<VectorInfo>(info); }
  [[nodiscard]] constexpr auto IsMatrix() const -> bool { return std::holds_alternative<MatrixInfo>(info); }
  template <typename T> [[nodiscard]] constexpr auto GetInfo() const -> const T & { return std::get<T>(info); }
  template <typename T> [[nodiscard]] constexpr auto GetInfoPtr() const -> const T * {
    if (std::holds_alternative<T>(info)) {
      return &std::get<T>(info);
    }
    return nullptr;
  }
  template <typename T> [[nodiscard]] constexpr auto Is() const -> bool { return std::holds_alternative<T>(info); }
  [[nodiscard]] auto GetOffset() const -> uint32_t {
    return std::visit([](const auto &data) -> uint32_t { return data.offset; }, info);
  }
  [[nodiscard]] auto GetArraySize() const -> uint32_t { return arraySize; }
  [[nodiscard]] constexpr auto GetSize() const -> uint32_t {
    return std::visit([](const auto &data) -> uint32_t { return data.size; }, info);
  }

  ResourceVariant info;

  [[nodiscard]] auto GetTypename() const -> std::string_view;

  [[nodiscard]] auto ToString() const -> std::string;
};

constexpr auto SlangResourceAccessToVkAccessFlags_Buffer(SlangResourceAccess access) -> VkAccessFlags2;
constexpr auto SlangResourceAccessToVkAccessFlags_Sampler(SlangResourceAccess access) -> VkAccessFlags2;

struct FlatteningState {
  VkDeviceSize size{};

  ResourceKey *currentKey = nullptr;
  uint32_t idx{};
  uint32_t currentOffset{};
  Standard std = Standard::Std140;
};

auto ResourceInfoToBufferFormat(const ResourceInfo &info, Standard std) -> Result<std::variant<Graphics::BufferFormat, Graphics::BufferComponent>>;

struct ShaderReflection {
  VkDeviceSize size;

  std::vector<ResourceInfo> keyToInfo;
  std::unordered_map<ResourceKey, ResolvedKey, ResourceKeyHash> keyMapping;

  std::vector<ResourceInfo> resources;
  BufferInfo globals;
  Graphics::BufferFormat globalBufferFormat;
  bool hasGlobals{false};

  // NOLINTNEXTLINE
  auto ConstructUBOStruct(uint32_t set, uint32_t binding) -> Error;
  auto FlattenReflection() -> Error;

private:
  auto FlattenResource(ResourceInfo &resource, FlatteningState &state, std::vector<ResourceInfo> &children, bool mapToGlobals) -> Error;
};

auto ReflectShader(const Graphics::GraphicsContext &context, slang::ProgramLayout *programLayout, ShaderReflection &outReflection) -> Error;

} // namespace Graphics::Reflect