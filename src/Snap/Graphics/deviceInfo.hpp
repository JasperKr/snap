#include "Libraries/vma.hpp"
#include <sstream>
#include <string>

#include <vulkan/vulkan.h>

namespace Graphics::Info {
struct GpuInfo {
  uint32_t vendorID;
  uint32_t deviceID;
  std::string deviceName;
};

inline auto GetAMDShaderCorePropertiesString(VkPhysicalDevice phys) -> std::string {
  VkPhysicalDeviceShaderCorePropertiesAMD amdProps{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_AMD};

  VkPhysicalDeviceProperties2 props{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  };
  props.pNext = &amdProps;

  vkGetPhysicalDeviceProperties2(phys, &props);

  std::ostringstream stream;
  stream << "VkPhysicalDeviceShaderCorePropertiesAMD\n";
  stream << "  shaderEngineCount: " << amdProps.shaderEngineCount << "\n";
  stream << "  shaderArraysPerEngineCount: " << amdProps.shaderArraysPerEngineCount << "\n";
  stream << "  computeUnitsPerShaderArray: " << amdProps.computeUnitsPerShaderArray << "\n";
  stream << "  simdPerComputeUnit: " << amdProps.simdPerComputeUnit << "\n";
  stream << "  wavefrontsPerSimd: " << amdProps.wavefrontsPerSimd << "\n";
  stream << "  wavefrontSize: " << amdProps.wavefrontSize << "\n";
  stream << "  sgprsPerSimd: " << amdProps.sgprsPerSimd << "\n";
  stream << "  minSgprAllocation: " << amdProps.minSgprAllocation << "\n";
  stream << "  maxSgprAllocation: " << amdProps.maxSgprAllocation << "\n";
  stream << "  sgprAllocationGranularity: " << amdProps.sgprAllocationGranularity << "\n";
  stream << "  vgprsPerSimd: " << amdProps.vgprsPerSimd << "\n";
  stream << "  minVgprAllocation: " << amdProps.minVgprAllocation << "\n";
  stream << "  maxVgprAllocation: " << amdProps.maxVgprAllocation << "\n";
  stream << "  vgprAllocationGranularity: " << amdProps.vgprAllocationGranularity << "\n";

  return stream.str();
}

inline auto GetGpuVendorString(uint32_t vendorID) -> std::string {
  switch (vendorID) {
  case 0x1002: // NOLINT
    return "AMD";
  case 0x10DE: // NOLINT
    return "NVIDIA";
  case 0x8086: // NOLINT
    return "Intel";
  case 0x13B5: // NOLINT
    return "ARM";
  case 0x5143: // NOLINT
    return "Qualcomm";
  default:
    return "Unknown";
  }
}

inline auto GetGpuInfo(VkPhysicalDevice phys) -> GpuInfo {
  VkPhysicalDeviceProperties2 props{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  };
  vkGetPhysicalDeviceProperties2(phys, &props);

  GpuInfo info{};
  info.vendorID = props.properties.vendorID;
  info.deviceID = props.properties.deviceID;
  info.deviceName = std::string(static_cast<const char *>(props.properties.deviceName));

  return info;
}

inline auto GetGpuDriverVersionString(VkPhysicalDevice phys) -> std::string {
  VkPhysicalDeviceProperties2 props{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  };
  vkGetPhysicalDeviceProperties2(phys, &props);

  uint32_t major = VK_VERSION_MAJOR(props.properties.driverVersion);
  uint32_t minor = VK_VERSION_MINOR(props.properties.driverVersion);
  uint32_t patch = VK_VERSION_PATCH(props.properties.driverVersion);

  std::ostringstream stream;
  stream << major << "." << minor << "." << patch;

  return stream.str();
}

inline auto GetGpuApiVersionString(VkPhysicalDevice phys) -> std::string {
  VkPhysicalDeviceProperties2 props{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
  };
  vkGetPhysicalDeviceProperties2(phys, &props);

  uint32_t major = VK_VERSION_MAJOR(props.properties.apiVersion);
  uint32_t minor = VK_VERSION_MINOR(props.properties.apiVersion);
  uint32_t patch = VK_VERSION_PATCH(props.properties.apiVersion);

  std::ostringstream stream;
  stream << major << "." << minor << "." << patch;

  return stream.str();
}

inline auto GetGpuInfoString(VkPhysicalDevice phys) -> std::string {
  GpuInfo info = GetGpuInfo(phys);
  std::ostringstream stream;

  stream << "GPU Info:\n";
  stream << "  Device Name: " << info.deviceName << "\n";
  stream << "  Vendor: " << GetGpuVendorString(info.vendorID) << " (" << std::hex << info.vendorID << std::dec << ")\n";
  stream << "  Device ID: " << std::hex << info.deviceID << std::dec << "\n";
  stream << "  Driver Version: " << GetGpuDriverVersionString(phys) << "\n";
  stream << "  API Version: " << GetGpuApiVersionString(phys) << "\n";

  if (info.vendorID == 0x1002) { // NOLINT
    stream << GetAMDShaderCorePropertiesString(phys);
  }

  return stream.str();
}

} // namespace Graphics::Info