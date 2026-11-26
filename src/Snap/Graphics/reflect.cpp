#include "reflect.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/descriptorSlots.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Graphics/graphicsState.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "slang/slang.h"

#include "vulkan/vulkan_core.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <public/tracy/Tracy.hpp>
#include <variant>
#include <vector>

namespace Graphics::Reflect {

auto SetupType(slang::TypeLayoutReflection *baseType) -> Result<std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo>> {
  switch (baseType->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    auto structInfo = StructInfo{};

    structInfo.size = static_cast<uint32_t>(baseType->getSize());
    structInfo.alignment = static_cast<uint32_t>(baseType->getAlignment());

    auto fieldCount = baseType->getFieldCount();

    for (int i = 0; i < fieldCount; ++i) {
      auto *fieldVariable = baseType->getFieldByIndex(i);
      auto *fieldType = fieldVariable->getTypeLayout();

      switch (fieldType->getKind()) {
      case slang::TypeReflection::Kind::Scalar: {
        auto scalarInfo = ScalarInfo{};

        scalarInfo.size = static_cast<uint32_t>(fieldType->getSize());
        scalarInfo.offset = static_cast<uint32_t>(fieldVariable->getOffset());
        scalarInfo.type = FromScalarType(fieldType->getScalarType());
        ResourceInfo fieldInfo(fieldVariable->getName(), scalarInfo);

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Vector: {
        auto vectorInfo = VectorInfo{};

        vectorInfo.size = static_cast<uint32_t>(fieldType->getSize());
        vectorInfo.offset = static_cast<uint32_t>(fieldVariable->getOffset());
        vectorInfo.scalarType = FromScalarType(fieldType->getScalarType());
        vectorInfo.vectorType = ToVectorType(fieldType->getElementCount());

        ResourceInfo fieldInfo(fieldVariable->getName(), vectorInfo);

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Matrix: {
        auto matrixInfo = MatrixInfo{};

        matrixInfo.size = static_cast<uint32_t>(fieldType->getSize());
        matrixInfo.offset = static_cast<uint32_t>(fieldVariable->getOffset());
        matrixInfo.matrixType = ToMatrixType(fieldType->getRowCount(), fieldType->getColumnCount());

        ResourceInfo fieldInfo(fieldVariable->getName(), matrixInfo);

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      case slang::TypeReflection::Kind::Struct: {
        auto structFieldInfo = CHECK_RES(SetupType(fieldType));

        ResourceInfo fieldInfo(fieldVariable->getName());

        if (std::holds_alternative<StructInfo>(structFieldInfo)) {
          std::get<StructInfo>(structFieldInfo).name = fieldVariable->getName();
          fieldInfo.info = std::get<StructInfo>(structFieldInfo);
        } else if (std::holds_alternative<ScalarInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<ScalarInfo>(structFieldInfo);
        } else if (std::holds_alternative<VectorInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<VectorInfo>(structFieldInfo);
        } else if (std::holds_alternative<MatrixInfo>(structFieldInfo)) {
          fieldInfo.info = std::get<MatrixInfo>(structFieldInfo);
        } else {
          return Error::Create("Unsupported struct field type in nested struct "
                               "reflection.");
        }

        structInfo.fields.emplace_back(fieldInfo);

        break;
      }
      default: {
        return Error::Unexpectedf("Unsupported struct field type in Buffer struct reflection.");
      }
      }
    }

    return structInfo;
  }
  case slang::TypeReflection::Kind::Scalar: {
    auto scalarInfo = ScalarInfo{};
    scalarInfo.size = static_cast<uint32_t>(baseType->getSize());
    scalarInfo.offset = static_cast<uint32_t>(baseType->getElementVarLayout()->getOffset());
    scalarInfo.type = FromScalarType(baseType->getScalarType());

    return scalarInfo;
  }
  case slang::TypeReflection::Kind::Vector: {
    auto vectorInfo = VectorInfo{};

    vectorInfo.size = static_cast<uint32_t>(baseType->getSize());
    vectorInfo.offset = static_cast<uint32_t>(baseType->getElementVarLayout()->getOffset());
    vectorInfo.scalarType = FromScalarType(baseType->getScalarType());
    vectorInfo.vectorType = ToVectorType(baseType->getElementCount());

    return vectorInfo;
  }
  case slang::TypeReflection::Kind::Matrix: {
    auto matrixInfo = MatrixInfo{};

    matrixInfo.size = static_cast<uint32_t>(baseType->getSize());
    matrixInfo.offset = static_cast<uint32_t>(baseType->getElementVarLayout()->getOffset());
    matrixInfo.matrixType = ToMatrixType(baseType->getRowCount(), baseType->getColumnCount());

    return matrixInfo;
  }
  default: {
    auto kindInt = static_cast<int>(baseType->getKind());
    PrintError("Unsupported type kind in SetupVariant: {}", kindInt);
    PrintError("Field name: {}", baseType->getName() == nullptr ? "Unnamed" : baseType->getName());
    PrintError("Type name: {}", baseType->getType()->getName() == nullptr ? "Unnamed" : baseType->getType()->getName());
    return Error::Unexpectedf("Unsupported buffer element type in Buffer reflection: {}", kindInt);
  }
  }

  int kind = static_cast<int>(baseType->getKind());

  return Error::Unexpectedf("Unsupported type layout kind for Buffer reflection: {}", kind);
}

auto SetupVariant(slang::VariableLayoutReflection *layout) -> Result<std::variant<StructInfo, ScalarInfo, VectorInfo, MatrixInfo>> {
  auto *baseType = layout->getTypeLayout();

  auto result = CHECK_RES(SetupType(baseType));

  if (baseType->getKind() == slang::TypeReflection::Kind::Struct) {
    std::get<StructInfo>(result).name = layout->getName();
  }

  return result;
}

inline auto SlangImageFormatToVkFormat(SlangImageFormat format) {
  switch (format) {
  case SLANG_IMAGE_FORMAT_unknown:
    return VK_FORMAT_UNDEFINED;
  case SLANG_IMAGE_FORMAT_rgba32f:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case SLANG_IMAGE_FORMAT_rgba16f:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case SLANG_IMAGE_FORMAT_rg32f:
    return VK_FORMAT_R32G32_SFLOAT;
  case SLANG_IMAGE_FORMAT_rg16f:
    return VK_FORMAT_R16G16_SFLOAT;
  case SLANG_IMAGE_FORMAT_r11f_g11f_b10f:
    return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
  case SLANG_IMAGE_FORMAT_r32f:
    return VK_FORMAT_R32_SFLOAT;
  case SLANG_IMAGE_FORMAT_r16f:
    return VK_FORMAT_R16_SFLOAT;
  case SLANG_IMAGE_FORMAT_rgba16:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case SLANG_IMAGE_FORMAT_rgb10_a2:
    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
  case SLANG_IMAGE_FORMAT_rgba8:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case SLANG_IMAGE_FORMAT_rg16:
    return VK_FORMAT_R16G16_UNORM;
  case SLANG_IMAGE_FORMAT_rg8:
    return VK_FORMAT_R8G8_UNORM;
  case SLANG_IMAGE_FORMAT_r16:
    return VK_FORMAT_R16_UNORM;
  case SLANG_IMAGE_FORMAT_r8:
    return VK_FORMAT_R8_UNORM;
  case SLANG_IMAGE_FORMAT_rgba16_snorm:
    return VK_FORMAT_R16G16B16A16_SNORM;
  case SLANG_IMAGE_FORMAT_rgba8_snorm:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case SLANG_IMAGE_FORMAT_rg16_snorm:
    return VK_FORMAT_R16G16_SNORM;
  case SLANG_IMAGE_FORMAT_rg8_snorm:
    return VK_FORMAT_R8G8_SNORM;
  case SLANG_IMAGE_FORMAT_r16_snorm:
    return VK_FORMAT_R16_SNORM;
  case SLANG_IMAGE_FORMAT_r8_snorm:
    return VK_FORMAT_R8_SNORM;
  case SLANG_IMAGE_FORMAT_rgba32i:
    return VK_FORMAT_R32G32B32A32_SINT;
  case SLANG_IMAGE_FORMAT_rgba16i:
    return VK_FORMAT_R16G16B16A16_SINT;
  case SLANG_IMAGE_FORMAT_rgba8i:
    return VK_FORMAT_R8G8B8A8_SINT;
  case SLANG_IMAGE_FORMAT_rg32i:
    return VK_FORMAT_R32G32_SINT;
  case SLANG_IMAGE_FORMAT_rg16i:
    return VK_FORMAT_R16G16_SINT;
  case SLANG_IMAGE_FORMAT_rg8i:
    return VK_FORMAT_R8G8_SINT;
  case SLANG_IMAGE_FORMAT_r32i:
    return VK_FORMAT_R32_SINT;
  case SLANG_IMAGE_FORMAT_r16i:
    return VK_FORMAT_R16_SINT;
  case SLANG_IMAGE_FORMAT_r8i:
    return VK_FORMAT_R8_SINT;
  case SLANG_IMAGE_FORMAT_rgba32ui:
    return VK_FORMAT_R32G32B32A32_UINT;
  case SLANG_IMAGE_FORMAT_rgba16ui:
    return VK_FORMAT_R16G16B16A16_UINT;
  case SLANG_IMAGE_FORMAT_rgb10_a2ui:
    return VK_FORMAT_A2R10G10B10_UINT_PACK32;
  case SLANG_IMAGE_FORMAT_rgba8ui:
    return VK_FORMAT_R8G8B8A8_UINT;
  case SLANG_IMAGE_FORMAT_rg32ui:
    return VK_FORMAT_R32G32_UINT;
  case SLANG_IMAGE_FORMAT_rg16ui:
    return VK_FORMAT_R16G16_UINT;
  case SLANG_IMAGE_FORMAT_rg8ui:
    return VK_FORMAT_R8G8_UINT;
  case SLANG_IMAGE_FORMAT_r32ui:
    return VK_FORMAT_R32_UINT;
  case SLANG_IMAGE_FORMAT_r16ui:
    return VK_FORMAT_R16_UINT;
  case SLANG_IMAGE_FORMAT_r8ui:
    return VK_FORMAT_R8_UINT;
  case SLANG_IMAGE_FORMAT_r64ui:
    return VK_FORMAT_R64_UINT;
  case SLANG_IMAGE_FORMAT_r64i:
    return VK_FORMAT_R64_SINT;
  case SLANG_IMAGE_FORMAT_bgra8:
    return VK_FORMAT_B8G8R8A8_UNORM;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

inline auto SlangStageToVkStage(SlangStage stage) -> VkShaderStageFlags {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case SLANG_STAGE_HULL:
    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
  case SLANG_STAGE_DOMAIN:
    return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
  case SLANG_STAGE_GEOMETRY:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case SLANG_STAGE_FRAGMENT:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case SLANG_STAGE_COMPUTE:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case SLANG_STAGE_MESH:
    return VK_SHADER_STAGE_MESH_BIT_EXT;
  case SLANG_STAGE_AMPLIFICATION:
    return VK_SHADER_STAGE_TASK_BIT_EXT;
  case SLANG_STAGE_RAY_GENERATION:
    return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  case SLANG_STAGE_ANY_HIT:
    return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
  case SLANG_STAGE_CLOSEST_HIT:
    return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  case SLANG_STAGE_MISS:
    return VK_SHADER_STAGE_MISS_BIT_KHR;
  case SLANG_STAGE_CALLABLE:
    return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
  default:
    return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto SetupResource(slang::VariableLayoutReflection *variableLayout) -> Result<ResourceInfo> {
  auto *typeLayout = variableLayout->getTypeLayout();
  auto kind = typeLayout->getKind();
  auto shape = typeLayout->getResourceShape();
  auto access = typeLayout->getResourceAccess();
  auto resourceInfo = ResourceInfo();

  // Masked out shape flags
  auto maskedShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;

  if (maskedShape == SLANG_TEXTURE_1D || maskedShape == SLANG_TEXTURE_2D || maskedShape == SLANG_TEXTURE_3D || maskedShape == SLANG_TEXTURE_CUBE ||
      maskedShape == SLANG_TEXTURE_BUFFER) {

    auto samplerInfo = SamplerInfo{};
    samplerInfo.set = variableLayout->getBindingSpace();
    samplerInfo.binding = variableLayout->getBindingIndex();
    samplerInfo.shape = shape;
    samplerInfo.access = access;
    samplerInfo.accessFlags = SlangResourceAccessToVkAccessFlags_Sampler(access);
    samplerInfo.format = SlangImageFormatToVkFormat(variableLayout->getImageFormat());

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = samplerInfo;
  } else if (maskedShape == SLANG_STRUCTURED_BUFFER || maskedShape == SLANG_BYTE_ADDRESS_BUFFER) {
    // SSBO
    auto bufferInfo = BufferInfo{};
    bufferInfo.name = variableLayout->getName();
    bufferInfo.set = variableLayout->getBindingSpace();
    bufferInfo.binding = variableLayout->getBindingIndex();
    bufferInfo.access = access;

    bufferInfo.accessFlags = SlangResourceAccessToVkAccessFlags_Buffer(access);
    bufferInfo.bufferType = BufferType::Storage;

    auto *bufferLayout = variableLayout->getTypeLayout();
    auto *elementVarLayout = bufferLayout->getElementVarLayout();

    if (elementVarLayout != nullptr) {
      const auto &resource = CHECK_RES(SetupResource(elementVarLayout));
      bufferInfo.info = std::make_shared<ResourceInfo>(resource);
    } else {
      StructInfo structInfo{};
      structInfo.size = static_cast<uint32_t>(bufferLayout->getSize());
      structInfo.alignment = static_cast<uint32_t>(bufferLayout->getAlignment());
      structInfo.name = "Unnamed Structured Buffer Struct";
      structInfo.offset = static_cast<uint32_t>(elementVarLayout->getOffset());

      for (int i = 0; i < bufferLayout->getFieldCount(); ++i) {
        auto *fieldVariable = bufferLayout->getFieldByIndex(i);
        auto variant = CHECK_RES(SetupVariant(fieldVariable));

        ResourceInfo fieldInfo(fieldVariable->getName());

        if (std::holds_alternative<StructInfo>(variant)) {
          fieldInfo.info = std::get<StructInfo>(variant);
        } else if (std::holds_alternative<ScalarInfo>(variant)) {
          fieldInfo.info = std::get<ScalarInfo>(variant);
        } else if (std::holds_alternative<VectorInfo>(variant)) {
          fieldInfo.info = std::get<VectorInfo>(variant);
        } else if (std::holds_alternative<MatrixInfo>(variant)) {
          fieldInfo.info = std::get<MatrixInfo>(variant);
        } else {
          return Error::Create("Unsupported struct field type in unnamed "
                               "structured buffer "
                               "reflection.");
        }

        structInfo.fields.emplace_back(fieldInfo);
      }

      bufferInfo.info->info = structInfo;
      bufferInfo.info->arraySize = bufferLayout->getElementCount();
      bufferInfo.info->offset = elementVarLayout->getOffset();
      bufferInfo.info->stages = SlangStageToVkStage(elementVarLayout->getStage());
    }

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = bufferInfo;
  } else if (maskedShape == SLANG_ACCELERATION_STRUCTURE) {
    auto asInfo = AccelerationStructureInfo{};
    asInfo.set = variableLayout->getBindingSpace();
    asInfo.binding = variableLayout->getBindingIndex();
    asInfo.shape = shape;
    asInfo.access = access;

    asInfo.accessFlags = SlangResourceAccessToVkAccessFlags_Buffer(access);

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = asInfo;
  } else {
    return Error::Unexpected("Unsupported resource shape in reflection.");
  }

  return resourceInfo;
}

auto SetupFromType(slang::VariableLayoutReflection *variableLayout, ShaderReflection &reflection) -> Result<ResourceInfo> {
  ZoneScoped;

  auto *typeLayout = variableLayout->getTypeLayout();
  auto kind = typeLayout->getKind();

  const auto *typeName = typeLayout->getType()->getName();
  const auto *paramName = variableLayout->getName();

  auto resourceInfo = ResourceInfo();

  switch (typeLayout->getKind()) {
  case slang::TypeReflection::Kind::Struct: {
    auto paramCount = typeLayout->getFieldCount();

    auto structInfo = StructInfo{};
    structInfo.name = typeName;
    structInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    structInfo.alignment = static_cast<uint32_t>(typeLayout->getAlignment());
    structInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());

    for (int i = 0; i < paramCount; i++) {
      auto *param = typeLayout->getFieldByIndex(i);
      auto kind = param->getTypeLayout()->getKind();

      auto info = CHECK_RES(SetupFromType(param, reflection));

      structInfo.fields.emplace_back(info);
    }

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = structInfo;
    break;
  }
  case slang::TypeReflection::Kind::ConstantBuffer: {
    auto category = variableLayout->getCategory();
    auto *bufferLayout = typeLayout->getElementVarLayout();

    if (bufferLayout == nullptr) {
      return Error::Unexpected("Constant buffer has no element variable layout.");
    }

    auto access = typeLayout->getResourceAccess();

    bool isPushConstant = (category == slang::ParameterCategory::PushConstantBuffer);

    auto bufferInfo = BufferInfo{};

    bufferInfo.name = variableLayout->getName();
    bufferInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    bufferInfo.set = variableLayout->getBindingSpace();
    bufferInfo.binding = variableLayout->getBindingIndex();
    bufferInfo.access = access;

    bufferInfo.accessFlags = SlangResourceAccessToVkAccessFlags_Buffer(access);

    bufferInfo.bufferType = isPushConstant ? BufferType::PushConstant : BufferType::Uniform;

    if (bufferInfo.bufferType == BufferType::Uniform) {
      bufferInfo.accessFlags = VK_ACCESS_2_UNIFORM_READ_BIT;
    }

    const auto &resource = CHECK_RES(SetupResource(bufferLayout));
    bufferInfo.info = std::make_shared<ResourceInfo>(resource);

    resourceInfo.name = variableLayout->getName();

    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = bufferInfo;
    break;
  }
  case slang::TypeReflection::Kind::Resource: {
    resourceInfo = CHECK_RES(SetupResource(variableLayout));
    break;
  }
  case slang::TypeReflection::Kind::Array: {
    const auto &info = CHECK_RES(SetupType(typeLayout->getElementTypeLayout()));
    resourceInfo.name = variableLayout->getName();
    resourceInfo.arraySize = typeLayout->getElementCount();

    if (resourceInfo.IsStruct()) {
      resourceInfo.info = std::get<StructInfo>(info);
    } else if (resourceInfo.IsScalar()) {
      resourceInfo.info = std::get<ScalarInfo>(info);
    } else if (resourceInfo.IsVector()) {
      resourceInfo.info = std::get<VectorInfo>(info);
    } else if (resourceInfo.IsMatrix()) {
      resourceInfo.info = std::get<MatrixInfo>(info);
    }

    PrintWarning("Array {}[{}]", variableLayout->getName(), typeLayout->getElementCount());

    break;
  }
  case slang::TypeReflection::Kind::Matrix: {
    auto matrixInfo = MatrixInfo{};
    matrixInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    matrixInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    matrixInfo.matrixType = ToMatrixType(typeLayout->getRowCount(), typeLayout->getColumnCount());

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = matrixInfo;

    break;
  }
  case slang::TypeReflection::Kind::Vector: {
    auto vectorInfo = VectorInfo{};

    vectorInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    vectorInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    vectorInfo.scalarType = FromScalarType(typeLayout->getScalarType());
    vectorInfo.vectorType = ToVectorType(typeLayout->getElementCount());

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = vectorInfo;

    break;
  }
  case slang::TypeReflection::Kind::Scalar: {
    auto scalarInfo = ScalarInfo{};

    scalarInfo.size = static_cast<uint32_t>(typeLayout->getSize());
    scalarInfo.offset = static_cast<uint32_t>(variableLayout->getOffset());
    scalarInfo.type = FromScalarType(typeLayout->getScalarType());

    resourceInfo.name = variableLayout->getName();
    resourceInfo.stages = SlangStageToVkStage(variableLayout->getStage());
    resourceInfo.info = scalarInfo;

    break;
  }
  default: {
    auto kindInt = static_cast<int>(typeLayout->getKind());
    return Error::Unexpectedf("Unsupported type layout kind for global UBO reflection: {}", kindInt);
  }
  }

  return resourceInfo;
}

auto ReflectShader(const Graphics::GraphicsContext &context, slang::ProgramLayout *programLayout, ShaderReflection &outReflection) -> Error {
  ZoneScoped;

  /// Search parameters ///
  auto parameterCount = programLayout->getParameterCount();

  for (int i = 0; i < parameterCount; ++i) {
    auto *param = programLayout->getParameterByIndex(i);
    auto *typeLayout = param->getTypeLayout();
    const auto *typeName = typeLayout->getType()->getName();
    const auto *paramName = param->getName();

    auto category = param->getCategory();

    outReflection.resources.emplace_back(CHECK_RES(SetupFromType(param, outReflection)));
  }

  auto *globalParamsLayout = programLayout->getGlobalParamsVarLayout();
  return outReflection.ConstructUBOStruct(globalParamsLayout->getBindingSpace(), globalParamsLayout->getBindingIndex());
}

auto BufferInfo::ToString() const -> std::string {
  // std::string result = "Buffer Name: " + name + " Type: ";
  std::string result = std::format("Buffer Name: {} Type: ", name);
  switch (bufferType) {
  case BufferType::Uniform:
    result += "Uniform";
    break;
  case BufferType::Storage:
    result += "Storage";
    break;
  case BufferType::PushConstant:
    result += "Push Constant";
    break;
  default:
    result += "Unknown";
    break;
  }
  result += " Size: " + std::to_string(size) + " Set: " + std::to_string(set) + " Binding: " + std::to_string(binding) + "\n";

  if (IsStruct()) {
    const auto &structInfo = std::get<StructInfo>(info->info);
    result += "  Struct Size: " + std::to_string(structInfo.size) + "\n";
    result += "  Fields:\n";
    for (const auto &field : structInfo.fields) {
      result += "    - " + field.ToString();
    }
  }

  return result;
}

auto ScalarTypeToVkFormat(ScalarType type, int count) -> VkFormat {
  switch (type) {
  case ScalarType::Float:
    switch (count) {
    case 1:
      return VK_FORMAT_R32_SFLOAT;
    case 2:
      return VK_FORMAT_R32G32_SFLOAT;
    case 3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      return VK_FORMAT_UNDEFINED;
    }
  case ScalarType::Int:
    switch (count) {
    case 1:
      return VK_FORMAT_R32_SINT;
    case 2:
      return VK_FORMAT_R32G32_SINT;
    case 3:
      return VK_FORMAT_R32G32B32_SINT;
    case 4:
      return VK_FORMAT_R32G32B32A32_SINT;
    default:
      return VK_FORMAT_UNDEFINED;
    }
  case ScalarType::UInt:
  case ScalarType::Bool:
    switch (count) {
    case 1:
      return VK_FORMAT_R32_UINT;
    case 2:
      return VK_FORMAT_R32G32_UINT;
    case 3:
      return VK_FORMAT_R32G32B32_UINT;
    case 4:
      return VK_FORMAT_R32G32B32A32_UINT;
    default:
      return VK_FORMAT_UNDEFINED;
    }
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

auto ResourceInfoToBufferFormat(const ResourceInfo &info, Standard std) -> Result<std::variant<Graphics::BufferFormat, Graphics::BufferComponent>> {
  if (info.IsBuffer()) {
    const auto &bufferInfo = std::get<BufferInfo>(info.info);

    if (bufferInfo.IsStruct()) {
      return ResourceInfoToBufferFormat(ResourceInfo(bufferInfo.name, info.stages, std::get<StructInfo>(bufferInfo.info->info)), std);
    }
    if (bufferInfo.IsScalar()) {
      return ResourceInfoToBufferFormat(ResourceInfo(bufferInfo.name, info.stages, std::get<ScalarInfo>(bufferInfo.info->info)), std);
    }
    if (bufferInfo.IsVector()) {
      return ResourceInfoToBufferFormat(ResourceInfo(bufferInfo.name, info.stages, std::get<VectorInfo>(bufferInfo.info->info)), std);
    }
    if (bufferInfo.IsMatrix()) {
      return ResourceInfoToBufferFormat(ResourceInfo(bufferInfo.name, info.stages, std::get<MatrixInfo>(bufferInfo.info->info)), std);
    }

    return Error::Unexpected("Unsupported buffer info type for buffer format conversion.");
  }

  if (info.IsStruct()) {
    const auto &structInfo = std::get<StructInfo>(info.info);

    std::vector<Graphics::BufferComponent> components;

    for (const auto &field : structInfo.fields) {
      auto fieldFormatResult = ResourceInfoToBufferFormat(field, std);

      if (!fieldFormatResult.has_value()) {
        continue; // Sampler or Buffer or whatever, skip it since it cannot be part of the buffer format
      }

      auto fieldFormatOrComponent = fieldFormatResult.value();

      if (std::holds_alternative<Graphics::BufferComponent>(fieldFormatOrComponent)) {
        components.emplace_back(std::get<Graphics::BufferComponent>(fieldFormatOrComponent));
      } else {
        auto bufferField = Graphics::BufferComponent{};
        bufferField.name = field.name;
        bufferField.format = std::get<Graphics::BufferFormat>(fieldFormatOrComponent);

        components.emplace_back(bufferField);
      }
    }

    return Graphics::BufferFormat(components, std);
  }

  if (info.IsScalar()) {
    const auto &scalarInfo = std::get<ScalarInfo>(info.info);
    auto component = Graphics::BufferComponent{};
    component.name = info.name;
    component.format = ScalarTypeToVkFormat(scalarInfo.type, 1);
    return component;
  }

  if (info.IsVector()) {
    const auto &vectorInfo = std::get<VectorInfo>(info.info);
    auto component = Graphics::BufferComponent{};
    component.name = info.name;
    component.format = ScalarTypeToVkFormat(vectorInfo.scalarType, VectorChannelCount(vectorInfo.vectorType));
    return component;
  }

  if (info.IsMatrix()) {
    const auto &matrixInfo = std::get<MatrixInfo>(info.info);
    auto component = Graphics::BufferComponent{};
    component.name = info.name;

    auto [colums, rows] = MatrixDimensions(matrixInfo.matrixType);
    component.format = ScalarTypeToVkFormat(ScalarType::Float, colums);
    component.arraySize = rows;

    return component;
  }

  if (info.IsAccelerationStructure()) {
    return Error::Success();
  }

  return Error::Unexpected("Unsupported resource info type for buffer format.");
}

auto ShaderReflection::ConstructUBOStruct(uint32_t set, uint32_t binding) -> Error {
  ResourceInfo globalUBOInfo{};
  globalUBOInfo.name = "Globals";
  globalUBOInfo.stages = VK_SHADER_STAGE_ALL;
  auto globalUBOStruct = StructInfo{};
  globalUBOStruct.name = "Globals";

  if (resources.size() == 0) {
    return {};
  }

  // Ordered erase to preserve field order, as this affects offsets in the UBO
  std::vector<ResourceInfo> filteredFields;
  for (const auto &field : resources) {
    if (!field.IsSampler() && !field.IsBuffer() && !field.IsAccelerationStructure()) {
      filteredFields.emplace_back(field);
    }
  }
  globalUBOStruct.fields = std::move(filteredFields);
  globalUBOInfo.info = globalUBOStruct;

  globals = {
      .name = "Globals",
      .set = set,
      .binding = binding,
      .access = SlangResourceAccess::SLANG_RESOURCE_ACCESS_READ,
      .bufferType = BufferType::Uniform,
      .info = std::make_shared<ResourceInfo>(globalUBOInfo),
  };

  const auto &infoResult = ResourceInfoToBufferFormat(globalUBOInfo, Standard::Std140);
  if (Error::IsError(infoResult)) {
    return infoResult.error();
  }
  auto formatOrComponent = infoResult.value();
  if (std::holds_alternative<Graphics::BufferFormat>(formatOrComponent)) {
    globalBufferFormat = std::get<Graphics::BufferFormat>(formatOrComponent);
  } else {
    return Error::Create("Global UBO struct must not be a literal.");
  }

  globals.accessFlags = VK_ACCESS_2_UNIFORM_READ_BIT;
  globals.size = globalBufferFormat.GetStride();
  hasGlobals = globals.size > 0;

  CHECK_ERR(FlattenReflection());

  return {};
}

// NOLINTNEXTLINE
auto ShaderReflection::FlattenResource(ResourceInfo &resource, FlatteningState &state, std::vector<ResourceInfo> &children, bool mapToGlobals) -> Error {
  if (resource.IsBuffer()) {
    const auto &bufferInfo = std::get<BufferInfo>(resource.info);
    resource.offset = bufferInfo.offset;

    switch (bufferInfo.bufferType) {
    case BufferType::Uniform:
      state.std = Standard::Std140;
      break;
    case BufferType::Storage:
      state.std = Standard::Std430;
      break;
    case BufferType::Unknown:
    case BufferType::PushConstant:
      state.std = Standard::Std140;
      break;
    }
  } else if (resource.IsSampler()) {
    const auto &samplerInfo = std::get<SamplerInfo>(resource.info);
  } else if (resource.IsAccelerationStructure()) {
    const auto &asInfo = std::get<AccelerationStructureInfo>(resource.info);
  }

  if (!resource.IsBuffer() && strcmp(resource.name, "Globals") != 0) {
    if (resource.arraySize > 1) {
      PrintWarning("Array var: {}[{}]", resource.name, state.idx);
      state.currentKey->emplace_back(resource.name, state.idx);
    } else {
      state.currentKey->emplace_back(resource.name);
    }
  }

  size_t baseAlign = 0;

  for (uint32_t i = 0; i < std::max(resource.GetArraySize(), 1U); i++) {
    resource.offset = resource.GetOffset() + state.currentOffset;

    if (resource.IsStruct()) {
      auto &structInfo = std::get<StructInfo>(resource.info);
      for (auto &field : structInfo.fields) {
        FlatteningState newState = state;
        newState.currentOffset += resource.offset;
        newState.idx = i;
        CHECK_ERR(FlattenResource(field, newState, children, mapToGlobals));
      }

    } else if (resource.IsBuffer()) {
      auto &bufferInfo = std::get<BufferInfo>(resource.info);
      FlatteningState newState = state;
      newState.idx = i;

      ResourceInfo info;
      if (std::holds_alternative<StructInfo>(bufferInfo.info->info)) {
        info.info = std::get<StructInfo>(bufferInfo.info->info);
      } else if (std::holds_alternative<ScalarInfo>(bufferInfo.info->info)) {
        info.info = std::get<ScalarInfo>(bufferInfo.info->info);
      } else if (std::holds_alternative<VectorInfo>(bufferInfo.info->info)) {
        info.info = std::get<VectorInfo>(bufferInfo.info->info);
      } else if (std::holds_alternative<MatrixInfo>(bufferInfo.info->info)) {
        info.info = std::get<MatrixInfo>(bufferInfo.info->info);
      } else {
        PrintError("Unsupported buffer info type for flattening.");
        continue;
      }
      info.stages = resource.stages;
      info.name = resource.name;

      CHECK_ERR(FlattenResource(info, newState, children, mapToGlobals));
    } else {
      if (mapToGlobals) {
        keyMapping.emplace(*state.currentKey, keyToInfo.size());
        keyToInfo.emplace_back(resource);
      }

      children.emplace_back(resource);
    }
  }

  if (!resource.IsBuffer() && strcmp(resource.name, "Globals") != 0) {
    state.currentKey->pop_back();
  }

  return {};
}

auto ShaderReflection::FlattenReflection() -> Error {
  for (auto &resource : resources) {
    // Only flatten samplers and buffers, skip other types
    if (!resource.IsSampler() && !resource.IsBuffer() && !resource.IsAccelerationStructure()) {
      continue;
    }

    bool isPushConstant = false;
    isPushConstant = resource.IsBuffer() && resource.GetInfo<BufferInfo>().bufferType == BufferType::PushConstant;

    // ERR_ASSERT(!isPushConstant);

    ResourceKey currentKey;
    FlatteningState state{
        .currentKey = &currentKey,
        .idx = 0,
    };

    std::vector<ResourceInfo> children;

    PrintInfo("Flattening: {} {}", resource.name, resource.arraySize);

    CHECK_ERR(FlattenResource(resource, state, children, false));

    for (auto &entry : children) {
      state.size = std::max<VkDeviceSize>(state.size, entry.GetOffset() + entry.GetSize());
    }
  }

  ResourceKey currentKey;
  FlatteningState state{
      .currentKey = &currentKey,
      .idx = 0,
  };

  ResourceInfo info;
  if (std::holds_alternative<StructInfo>(globals.info->info)) {
    info.info = std::get<StructInfo>(globals.info->info);
  } else if (std::holds_alternative<ScalarInfo>(globals.info->info)) {
    info.info = std::get<ScalarInfo>(globals.info->info);
  } else if (std::holds_alternative<VectorInfo>(globals.info->info)) {
    info.info = std::get<VectorInfo>(globals.info->info);
  } else if (std::holds_alternative<MatrixInfo>(globals.info->info)) {
    info.info = std::get<MatrixInfo>(globals.info->info);
  } else {
    PrintError("Unsupported buffer info type for flattening.");
  }
  info.stages = VK_SHADER_STAGE_ALL;
  info.name = "Globals";

  std::vector<ResourceInfo> children;

  CHECK_ERR(FlattenResource(info, state, children, true));

  for (auto &entry : children) {
    state.size = std::max<VkDeviceSize>(state.size, entry.GetOffset() + entry.GetSize());
  }

  return {};
}

auto FromScalarType(slang::TypeReflection::ScalarType baseType) -> ScalarType {
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
auto ToVectorType(uint8_t elementCount) -> VectorType {
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
auto VectorChannelCount(VectorType vectorType) -> uint8_t {
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
auto MatrixDimensions(MatrixType matrixType) -> std::pair<uint8_t, uint8_t> {
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
auto ToMatrixType(uint8_t rowCount, uint8_t columnCount) -> MatrixType {
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
auto ResourceKeyToString(const Graphics::ResourceKey &key) -> std::string {
  std::string result;
  for (const auto &part : key) {
    if (!result.empty()) {
      result += ".";
    }
    result += part.ToString();
  }
  return result;
}
[[nodiscard]] auto ResourceInfo::GetTypename() const -> std::string_view {
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
[[nodiscard]] auto ResourceInfo::ToString() const -> std::string {
  auto result = std::format("Resource Name: {} Type: {} Offset: {}, Size: {}, Array Size: {}\n", name, GetTypename(), GetOffset(), GetSize(), GetArraySize());

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
constexpr auto SlangResourceAccessToVkAccessFlags_Buffer(SlangResourceAccess access) -> VkAccessFlags2 {
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
constexpr auto SlangResourceAccessToVkAccessFlags_Sampler(SlangResourceAccess access) -> VkAccessFlags2 {
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
[[nodiscard]] constexpr auto BufferInfo::IsStruct() const -> bool { return std::holds_alternative<StructInfo>(info->info); }
[[nodiscard]] constexpr auto BufferInfo::IsScalar() const -> bool { return std::holds_alternative<ScalarInfo>(info->info); }
[[nodiscard]] constexpr auto BufferInfo::IsVector() const -> bool { return std::holds_alternative<VectorInfo>(info->info); }
[[nodiscard]] constexpr auto BufferInfo::IsMatrix() const -> bool { return std::holds_alternative<MatrixInfo>(info->info); }
} // namespace Graphics::Reflect