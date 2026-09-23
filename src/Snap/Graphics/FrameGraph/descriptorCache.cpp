#include "descriptorCache.hpp"
#include "Graphics/allocations.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"

namespace Graphics {

auto DescriptorCache::Initialize(const GraphicsContext &context) -> Error {
  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 0;
  layoutInfo.pBindings = nullptr;
  CHECK_NEW_ERR(vkCreateDescriptorSetLayout(
      context.device, &layoutInfo, GetAllocationCallbacks(), &emptySetLayout));

  return {};
}

auto DescriptorCache::DeInitialize(const GraphicsContext &context) -> void {
  for (auto &layouts : descriptorSetLayoutCache) {
    vkDestroyDescriptorSetLayout(context.device, layouts.second,
                                 GetAllocationCallbacks());
  }

  vkDestroyDescriptorSetLayout(context.device, emptySetLayout,
                               GetAllocationCallbacks());
  descriptorSetLayoutCache.clear();
}

auto DescriptorCache::GetDescriptorSetLayout(
    const DescriptorSetLayoutKey &layoutKey, const GraphicsContext &context)
    -> Result<VkDescriptorSetLayout> {
  ZoneScoped;

  const auto &bindings = layoutKey.bindings;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

  std::lock_guard<std::mutex> lock(descriptorSetLayoutCacheMutex);

  if (descriptorSetLayoutCache.contains(layoutKey)) {
    descriptorSetLayout = descriptorSetLayoutCache.at(layoutKey);
  } else {
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    CHECK_NEW_ERR(vkCreateDescriptorSetLayout(context.device, &layoutInfo,
                                              GetAllocationCallbacks(),
                                              &descriptorSetLayout));

    descriptorSetLayoutCache[layoutKey] = descriptorSetLayout;
  }

  return descriptorSetLayout;
}

auto GetDescriptorCache() -> DescriptorCache & {
  static DescriptorCache cache{};

  return cache;
}

} // namespace Graphics