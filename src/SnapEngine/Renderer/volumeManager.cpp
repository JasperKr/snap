#include "volumeManager.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/volume.hpp"
#include <cstdint>
#include <span>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

auto VolumeManager::Initialize(const Graphics::GraphicsContext &context) -> Error {
  VolumeTLAS = CHECK_RES(Graphics::TLAS::Create(context, "Volumes TLAS"));

  CubeBuffer = CHECK_RES(Graphics::Buffer::Create(context, {
                                                               .size = sizeof(VkAabbPositionsKHR),
                                                               .usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
                                                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                                                               .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                               .stagingBuffer = false,
                                                               .persistentMapping = false,
                                                               .debugName = "Volume cube bounds",
                                                           }));

  auto data = VkAabbPositionsKHR{
      .minX = -0.5,
      .minY = -0.5,
      .minZ = -0.5,
      .maxX = 0.5,
      .maxY = 0.5,
      .maxZ = 0.5,
  };

  auto span = std::span<VkAabbPositionsKHR>(&data, 1);

  CHECK_ERR(CubeBuffer->SetData(context, span));

  InstanceBLAS = CHECK_RES(Graphics::BLAS::Create(context, CubeBuffer, 1));
  VolumeInstanceData = CHECK_RES(Graphics::Buffer::Create(context, {
                                                                       .size = sizeof(VolumeData) * InitialVolumeBufferSize,
                                                                       .usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
                                                                       .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                                       .stagingBuffer = false,
                                                                       .persistentMapping = false,
                                                                       .debugName = "Volume data",
                                                                   }));

  return {};
}

auto VolumeManager::GetTLAS() -> Ref<Graphics::TLAS> { return VolumeTLAS; }
auto VolumeManager::GetVolumeInstanceData() -> Ref<Graphics::Buffer> { return VolumeInstanceData; }
auto VolumeManager::QueueRebuild() -> void { rebuildQueued = true; }
auto VolumeManager::AddInstance(const Graphics::GraphicsContext &context, const Volume &volume, Math::Vec3 min, Math::Vec3 max) -> Result<uint32_t> {
  Math::Vec3 position = (min + max) * 0.5F;
  Math::Vec3 scale = max - min;

  Math::Matrix4x4 transform = Math::Matrix4x4::TransformationMatrix(position, scale, Math::Quaternion());

  uint32_t index = CHECK_RES(VolumeTLAS->AddInstance(InstanceBLAS, transform, VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR));

  // No resizing currently, who has more than 1024 volumes anyways
  CHECK_ERR(index < InitialVolumeBufferSize);

  uint32_t textureAddress = 0UL;
  uint32_t samplerAddress = 0UL;

  if (volume.volumeTexture != nullptr) {
    textureAddress = CHECK_RES(volume.volumeTexture->GetAddress(context, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE));
    samplerAddress = CHECK_RES(volume.volumeTexture->GetSampler(context));
  }

  VolumeData instanceData = {
      .density = volume.density,
      .anisotropy = volume.anisotropy,
      .volumeDensityTexture = textureAddress,
      .volumeDensitySampler = samplerAddress,
      .min = Math::Vec4(min, 0.0F),
      .max = Math::Vec4(max, 0.0F),
      .color = volume.color,
  };

  auto span = std::span<VolumeData>(&instanceData, 1);
  CHECK_ERR(VolumeInstanceData->SetData(context, span, sizeof(VolumeData) * index));

  rebuildQueued = true;

  return index;
}

auto VolumeManager::UpdateInstance(const Graphics::GraphicsContext &context, uint32_t index, const Volume &volume, Math::Vec3 min, Math::Vec3 max) -> Error {
  Math::Vec3 position = (min + max) * 0.5F;
  Math::Vec3 scale = max - min;

  Math::Matrix4x4 transform = Math::Matrix4x4::TransformationMatrix(position, scale, Math::Quaternion());

  VolumeTLAS->UpdateInstance(index, transform);

  uint32_t textureAddress = 0UL;
  uint32_t samplerAddress = 0UL;

  if (volume.volumeTexture != nullptr) {
    textureAddress = CHECK_RES(volume.volumeTexture->GetAddress(context, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE));
    samplerAddress = CHECK_RES(volume.volumeTexture->GetSampler(context));
  }

  VolumeData instanceData = {
      .density = volume.density,
      .anisotropy = volume.anisotropy,
      .volumeDensityTexture = textureAddress,
      .volumeDensitySampler = samplerAddress,
      .min = Math::Vec4(min, 0.0F),
      .max = Math::Vec4(max, 0.0F),
      .color = volume.color,
  };

  auto span = std::span<VolumeData>(&instanceData, 1);
  CHECK_ERR(VolumeInstanceData->SetData(context, span, sizeof(VolumeData) * index));

  return {};
}

auto VolumeManager::RemoveInstance(uint32_t index) -> void { VolumeTLAS->RemoveInstance(index); }

auto VolumeManager::Update(const Graphics::GraphicsContext &context) -> Error {
  if (VolumeTLAS->GetInstances().empty()) {
    return {};
  }

  CHECK_ERR(rebuildQueued ? VolumeTLAS->Rebuild(context) : VolumeTLAS->Refit(context));
  rebuildQueued = false;

  return {};
}

} // namespace Engine::Renderer