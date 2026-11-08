#pragma once

#include "Graphics/reflect.hpp"
#include "Modules/color.hpp"
#include "Modules/error.hpp"
#include <cstdint>
#include <cstring>

#include <vulkan/vulkan_core.h>

namespace Wrap::Utils {

inline auto SetData(double luaNumber, uint8_t *dataPtr, VkFormat format) -> Error {
  switch (format) {
  // UINT formats
  // 8-bit
  case VK_FORMAT_R8_UINT:
  case VK_FORMAT_R8G8_UINT:
  case VK_FORMAT_R8G8B8_UINT:
  case VK_FORMAT_B8G8R8_UINT:
  case VK_FORMAT_R8G8B8A8_UINT:
  case VK_FORMAT_B8G8R8A8_UINT: {
    auto value = static_cast<uint8_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(uint8_t));
    break;
  }
  // 16-bit
  case VK_FORMAT_R16_UINT:
  case VK_FORMAT_R16G16_UINT:
  case VK_FORMAT_R16G16B16_UINT:
  case VK_FORMAT_R16G16B16A16_UINT: {
    auto value = static_cast<uint16_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(uint16_t));
    break;
  }
  // 32-bit
  case VK_FORMAT_R32_UINT:
  case VK_FORMAT_R32G32_UINT:
  case VK_FORMAT_R32G32B32_UINT:
  case VK_FORMAT_R32G32B32A32_UINT: {
    auto value = static_cast<uint32_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(uint32_t));
    break;
  }

  // SINT formats
  // 8-bit
  case VK_FORMAT_R8_SINT:
  case VK_FORMAT_R8G8_SINT:
  case VK_FORMAT_R8G8B8_SINT:
  case VK_FORMAT_B8G8R8_SINT:
  case VK_FORMAT_R8G8B8A8_SINT:
  case VK_FORMAT_B8G8R8A8_SINT: {
    auto value = static_cast<int8_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(int8_t));
    break;
  }
  // 16-bit
  case VK_FORMAT_R16_SINT:
  case VK_FORMAT_R16G16_SINT:
  case VK_FORMAT_R16G16B16_SINT:
  case VK_FORMAT_R16G16B16A16_SINT: {
    auto value = static_cast<int16_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(int16_t));
    break;
  }
  // 32-bit
  case VK_FORMAT_R32_SINT:
  case VK_FORMAT_R32G32_SINT:
  case VK_FORMAT_R32G32B32_SINT:
  case VK_FORMAT_R32G32B32A32_SINT: {
    auto value = static_cast<int32_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(int32_t));
    break;
  }

  // UNORM formats
  // 8-bit
  case VK_FORMAT_R8_UNORM:
  case VK_FORMAT_R8G8_UNORM:
  case VK_FORMAT_R8G8B8_UNORM:
  case VK_FORMAT_B8G8R8_UNORM:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_B8G8R8A8_UNORM: {
    constexpr float UNORM_8BIT_MAX = 255.0F;
    luaNumber = std::clamp(luaNumber, 0.0, 1.0);
    auto value = static_cast<uint8_t>(luaNumber * UNORM_8BIT_MAX);
    std::memcpy(dataPtr, &value, sizeof(uint8_t));
    break;
  }
  // 16-bit
  case VK_FORMAT_R16_UNORM:
  case VK_FORMAT_R16G16_UNORM:
  case VK_FORMAT_R16G16B16_UNORM:
  case VK_FORMAT_R16G16B16A16_UNORM: {
    constexpr float UNORM_16BIT_MAX = 65535.0F;
    luaNumber = std::clamp(luaNumber, 0.0, 1.0);
    auto value = static_cast<uint16_t>(luaNumber * UNORM_16BIT_MAX);
    std::memcpy(dataPtr, &value, sizeof(uint16_t));
    break;
  }

  // SNORM formats
  // 8-bit
  case VK_FORMAT_R8_SNORM:
  case VK_FORMAT_R8G8_SNORM:
  case VK_FORMAT_R8G8B8_SNORM:
  case VK_FORMAT_B8G8R8_SNORM:
  case VK_FORMAT_R8G8B8A8_SNORM:
  case VK_FORMAT_B8G8R8A8_SNORM: {
    constexpr float SNORM_8BIT_MAX = 127.0F;
    luaNumber = std::clamp(luaNumber, -1.0, 1.0);
    auto value = static_cast<int8_t>(luaNumber * SNORM_8BIT_MAX);
    std::memcpy(dataPtr, &value, sizeof(int8_t));
    break;
  }
  // 16-bit
  case VK_FORMAT_R16_SNORM:
  case VK_FORMAT_R16G16_SNORM:
  case VK_FORMAT_R16G16B16_SNORM:
  case VK_FORMAT_R16G16B16A16_SNORM: {
    constexpr float SNORM_16BIT_MAX = 32767.0F;
    luaNumber = std::clamp(luaNumber, -1.0, 1.0);
    auto value = static_cast<int16_t>(luaNumber * SNORM_16BIT_MAX);
    std::memcpy(dataPtr, &value, sizeof(int16_t));
    break;
  }

  // SFLOAT formats
  // 16-bit
  case VK_FORMAT_R16_SFLOAT:
  case VK_FORMAT_R16G16_SFLOAT:
  case VK_FORMAT_R16G16B16_SFLOAT:
  case VK_FORMAT_R16G16B16A16_SFLOAT: {
    auto value = static_cast<numeric::float16_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(numeric::float16_t));
    break;
  }
  // 32-bit
  case VK_FORMAT_R32_SFLOAT:
  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R32G32B32_SFLOAT:
  case VK_FORMAT_R32G32B32A32_SFLOAT: {
    auto value = static_cast<float>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(float));
    break;
  }

  // SRGB formats
  // 8-bit
  case VK_FORMAT_R8_SRGB:
  case VK_FORMAT_R8G8_SRGB:
  case VK_FORMAT_R8G8B8_SRGB:
  case VK_FORMAT_B8G8R8_SRGB:
  case VK_FORMAT_R8G8B8A8_SRGB:
  case VK_FORMAT_B8G8R8A8_SRGB:

  // Undefined or unsupported
  case VK_FORMAT_UNDEFINED:
  default:
    return Error::Create("Unsupported format for SetData.");
    break;
  }

  return Error::Success();
}

inline auto SetData(double luaNumber, uint8_t *dataPtr, ::Graphics::Reflect::ScalarType format) -> Error {
  switch (format) {
  case ::Graphics::Reflect::ScalarType::Float: {
    auto value = static_cast<float>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(float));
    break;
  }
  case ::Graphics::Reflect::ScalarType::Int: {
    auto value = static_cast<int32_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(int32_t));
    break;
  }
  case ::Graphics::Reflect::ScalarType::UInt: {
    auto value = static_cast<uint32_t>(luaNumber);
    std::memcpy(dataPtr, &value, sizeof(uint32_t));
    break;
  }
  case ::Graphics::Reflect::ScalarType::Bool: {
    auto value = static_cast<uint32_t>(luaNumber != 0.0);
    std::memcpy(dataPtr, &value, sizeof(uint32_t));
    break;
  }
  case ::Graphics::Reflect::ScalarType::Unknown:
    return Error::Create("Unsupported format `Unknown` for SetData.");
  }

  return Error::Success();
}

} // namespace Wrap::Utils