#include "sampler.hpp"
#include "Graphics/descriptorHeap.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/error.hpp"
#include <public/tracy/Tracy.hpp>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

namespace Graphics {

std::unordered_map<SamplerDescription, VkDeviceSize, SamplerDescHash> SamplerCache; // NOLINT

auto GetOrCreateSampler(const GraphicsContext &context, const SamplerDescription &description) -> Result<VkDeviceSize> {
  ZoneScoped;

  auto iter = SamplerCache.find(description);
  if (iter != SamplerCache.end()) {
    return iter->second; // Return cached sampler
  }

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = description.magFilter;
  samplerInfo.minFilter = description.minFilter;
  samplerInfo.mipmapMode = description.mipmapMode;
  samplerInfo.addressModeU = description.addressModeU;
  samplerInfo.addressModeV = description.addressModeV;
  samplerInfo.addressModeW = description.addressModeW;
  samplerInfo.mipLodBias = description.mipLodBias;
  samplerInfo.anisotropyEnable = description.anisotropyEnable ? VK_TRUE : VK_FALSE;
  samplerInfo.maxAnisotropy = description.maxAnisotropy;
  samplerInfo.compareEnable = description.compareEnable ? VK_TRUE : VK_FALSE;
  samplerInfo.compareOp = description.compareOp;
  samplerInfo.minLod = description.minLod;
  samplerInfo.maxLod = description.maxLod;
  samplerInfo.borderColor = description.borderColor;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;

  VkDeviceSize sampler = CHECK_RES(DescriptorHeapManager::GetInstance().AllocateSampler(context, samplerInfo));

  SamplerCache[description] = sampler;
  return sampler;
}

auto DestroySamplers(const GraphicsContext &context) -> void {
  std::lock_guard<std::mutex> lock(Graphics::GraphicsContext::mutexes.device);
  for (auto &pair : SamplerCache) {
    DescriptorHeapManager::GetInstance().FreeSamplerResourceIndex(pair.second);
  }
  SamplerCache.clear();
}

auto SamplerDescHash::operator()(const SamplerDescription &desc) const -> size_t {
  Hash::Hasher hash;

  hash.Add(desc.magFilter);
  hash.Add(desc.minFilter);
  hash.Add(desc.mipmapMode);
  hash.Add(desc.addressModeU);
  hash.Add(desc.addressModeV);
  hash.Add(desc.addressModeW);
  hash.Add(desc.mipLodBias);
  hash.Add((int)desc.anisotropyEnable);
  hash.Add(desc.maxAnisotropy);
  hash.Add((int)desc.compareEnable);
  hash.Add(desc.compareOp);
  hash.Add(desc.minLod);
  hash.Add(desc.maxLod);
  hash.Add(desc.borderColor);

  return hash.Get();
}
} // namespace Graphics
