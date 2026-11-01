#include "volumeManager.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/volume.hpp"
#include <cstdint>
#include <span>
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

auto VolumeManager::Initialize(const Graphics::GraphicsContext &context)
    -> Error {
  VolumeTLAS = CHECK_RES(Graphics::TLAS::Create(context, "Volumes TLAS"));

  CubeBuffer = CHECK_RES(Graphics::Buffer::Create(
      context, {
                   .size = sizeof(float) * 8ULL,
                   .usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
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
  VolumeInstanceData = CHECK_RES(Graphics::Buffer::Create(
      context, {
                   .size = sizeof(VolumeData) * InitialVolumeBufferSize,
                   .usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_2_TRANSFER_DST_BIT,
                   .properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   .stagingBuffer = false,
                   .persistentMapping = false,
                   .debugName = "Volume data",
               }));

  return {};
}

auto VolumeManager::GetTLAS() -> Ref<Graphics::TLAS> { return VolumeTLAS; }
auto VolumeManager::QueueRebuild() -> void { rebuildQueued = true; }
auto VolumeManager::AddInstance(const Graphics::GraphicsContext &context,
                                const Volume &volume, Math::Vec3 min,
                                Math::Vec3 max) -> Result<uint32_t> {
  Math::Vec3 position = (min + max) * 0.5F;
  Math::Vec3 scale = max - min;

  Math::Matrix4x4 transform = Math::Matrix4x4::TransformationMatrix(
      position, scale, Math::Quaternion());

  uint32_t index = CHECK_RES(VolumeTLAS->AddInstance(InstanceBLAS, transform));

  // No resizing currently, who has more than 1024 volumes anyways
  CHECK_ERR(index < InitialVolumeBufferSize);

  uint32_t textureAddress = 0UL;
  uint32_t samplerAddress = 0UL;

  if (volume.volumeTexture != nullptr) {
    textureAddress = CHECK_RES(volume.volumeTexture->GetAddress(
        context, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE));
    samplerAddress = CHECK_RES(volume.volumeTexture->GetSampler(context));
  }

  VolumeData instanceData = {
      .density = volume.density,
      .anisotropy = volume.anisotropy,
      .volumeTexture = textureAddress,
      .volumeSampler = samplerAddress,
      .min = min,
      .padding_0 = {},
      .max = max,
      .padding_1 = {},
  };

  auto span = std::span<VolumeData>(&instanceData, 1);
  CHECK_ERR(
      VolumeInstanceData->SetData(context, span, sizeof(VolumeData) * index));

  return index;
}

auto VolumeManager::UpdateInstance(const Graphics::GraphicsContext &context,
                                   uint32_t index, const Volume &volume,
                                   Math::Vec3 min, Math::Vec3 max) -> Error {
  Math::Vec3 position = (min + max) * 0.5F;
  Math::Vec3 scale = max - min;

  Math::Matrix4x4 transform = Math::Matrix4x4::TransformationMatrix(
      position, scale, Math::Quaternion());

  VolumeTLAS->UpdateInstance(index, transform);

  uint32_t textureAddress = 0UL;
  uint32_t samplerAddress = 0UL;

  if (volume.volumeTexture != nullptr) {
    textureAddress = CHECK_RES(volume.volumeTexture->GetAddress(
        context, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE));
    samplerAddress = CHECK_RES(volume.volumeTexture->GetSampler(context));
  }

  VolumeData instanceData = {
      .density = volume.density,
      .anisotropy = volume.anisotropy,
      .volumeTexture = textureAddress,
      .volumeSampler = samplerAddress,
      .min = min,
      .padding_0 = {},
      .max = max,
      .padding_1 = {},
  };

  auto span = std::span<VolumeData>(&instanceData, 1);
  CHECK_ERR(
      VolumeInstanceData->SetData(context, span, sizeof(VolumeData) * index));

  return {};
}

auto VolumeManager::RemoveInstance(uint32_t index) -> void {
  VolumeTLAS->RemoveInstance(index);
}

auto VolumeManager::Update(const Graphics::GraphicsContext &context) -> Error {
  CHECK_ERR(rebuildQueued ? VolumeTLAS->Rebuild(context)
                          : VolumeTLAS->Refit(context));
  rebuildQueued = false;

  return {};
}

} // namespace Engine::Renderer