#pragma once

#include "Graphics/buffer.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/graphicsContext.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/volume.hpp"
#include <cstdint>

namespace Engine::Renderer {
constexpr VkDeviceSize InitialVolumeBufferSize = 1024UL;

struct VolumeManager {
  auto Initialize(const Graphics::GraphicsContext &context) -> Error;
  auto Update(const Graphics::GraphicsContext &context) -> Error;
  auto QueueRebuild() -> void;

  auto GetTLAS() -> Ref<Graphics::TLAS>;
  auto GetVolumeInstanceData() -> Ref<Graphics::Buffer>;

  auto AddInstance(const Graphics::GraphicsContext &context,
                   const Volume &volume, Math::Vec3 min, Math::Vec3 max)
      -> Result<uint32_t>;

  auto UpdateInstance(const Graphics::GraphicsContext &context, uint32_t index,
                      const Volume &volume, Math::Vec3 min, Math::Vec3 max)
      -> Error;

  auto RemoveInstance(uint32_t index) -> void;

private:
  // 1m x 1m x 1m cube to be instanced for each volume
  Ref<Graphics::BLAS> InstanceBLAS;
  Ref<Graphics::Buffer> CubeBuffer;

  Ref<Graphics::TLAS> VolumeTLAS;
  Ref<Graphics::Buffer> VolumeInstanceData;

  struct VolumeData {
    float density{};
    float anisotropy{};
    uint32_t volumeTexture{};
    uint32_t volumeSampler{};

    Math::Vec3 min;
    uint32_t padding_0{};

    Math::Vec3 max;
    uint32_t padding_1{};
  };

  bool rebuildQueued;
};

} // namespace Engine::Renderer