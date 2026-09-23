#pragma once

#include "Graphics/sampler.hpp"
#include "Libraries/vma.hpp"
#include "Modules/compressedImageData.hpp"
#include "Modules/error.hpp"
#include "Modules/image.hpp"
#include "Modules/imageData.hpp"
#include "Modules/localState.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <span>

#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Graphics {

struct Buffer;

enum class TextureType : uint8_t {
  DEFAULT = 0x00,
  VOLUME = 0x01,
  CUBEMAP = 0x02,
  ARRAY = 0x03,
  ENUM_MAX = 0xFF,
};

enum class WrapMode : uint8_t {
  REPEAT,
  MIRRORED_REPEAT,
  CLAMP,
  CLAMPONE,
  CLAMPZERO
};

const static Type LuaTextureType = Type("Texture");

enum class TextureUsage : uint8_t {
  Sampler,
  Storage,
  Attachment,
  TransferSrc,
  TransferDst,
  PresentSrc,
  Swapchain, // Just acquired from the swapchain
  Unknown,
};

enum class TextureMipmapOption : uint8_t {
  None,   // Do not create mipmaps.
  Init,   // Calculate a mip chain.
  Manual, // Allocate a mip chain.
};

extern std::unordered_map<std::pair<VkFormat, TextureType>, Ref<struct Texture>,
                          struct VkFormatTextureTypeHash>
    DefaultTextureCache;                    // NOLINT
extern std::mutex DefaultTextureCacheMutex; // NOLINT

auto UnloadModule() -> void;

struct CopyRegion {
  VkOffset3D srcOffset{};
  VkOffset3D dstOffset{};
  VkExtent3D extent{};
  uint32_t srcBaseArrayLayer = 0;
  uint32_t dstBaseArrayLayer = 0;
  uint32_t layerCount = 1;
  uint32_t srcBaseMipLevel = 0;
  uint32_t dstBaseMipLevel = 0;
  uint32_t mipLevelCount = 1;
};

struct ToBufferCopyRegion {
  VkOffset3D srcOffset{};
  VkExtent3D extent{};
  uint32_t srcBaseArrayLayer = 0;
  uint32_t layerCount = 1;
  uint32_t srcBaseMipLevel = 0;
  uint32_t mipLevelCount = 1;
  VkDeviceSize dstOffset = 0;
};

struct TextureCreationInfo {
  // Dimensions in texels
  VkExtent3D size{};

  // Amount of array layers
  uint32_t arrayLayers{1};

  // Texture format
  VkFormat format = VK_FORMAT_UNDEFINED;

  // Vulkan usage flags
  VkImageUsageFlags usage{};

  // Number of mipmap levels to allocate.
  int mipmapCount = 1;

  // Debug name
  std::string debugName;

  // Type of the texture (2D, Cubemap, etc.)
  TextureType textureType = TextureType::ENUM_MAX;
};

struct ImageState {
  VkPipelineStageFlagBits2 lastPipelineStage =
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  TextureUsage lastUsage = TextureUsage::Unknown;

  // Not for destruction, initialized to UINT64_MAX, since frame 0 is valid
  uint64_t lastUsedFrame = UINT64_MAX;
};

struct ImageMemory : Identifiable {
  ImageMemory() = default;
  ImageMemory(const ImageMemory &) = delete;
  auto operator=(const ImageMemory &) -> ImageMemory & = delete;
  ImageMemory(ImageMemory &&) noexcept = delete;
  auto operator=(ImageMemory &&) noexcept -> ImageMemory & = delete;
  ~ImageMemory();

  std::mutex mutex;

  VkImage image = VK_NULL_HANDLE;
  VmaAllocation memory = VK_NULL_HANDLE;
  std::string debugName;
  uint64_t sizeInBytes = 0;
  uint64_t lastUsedTimestamp{};

  VkFormat format = VK_FORMAT_UNDEFINED;
  size_t mipmapcount{1};
  size_t arrayLayers{1};
  VkImageUsageFlags usage{};

  // Actual state of the image. Should never be accessed in any graphics thread
  // Including the main thread. Only relevant to the reordering thread while reordering.
  mutable ImageState currentState;

  static ThreadLocalState<ImageState> ImageStateManager;
  [[nodiscard]] auto GetState() const -> ImageState &;

  auto UseAs(const GraphicsContext &context, TextureUsage newUsage,
             VkPipelineStageFlags2 stage,
             VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
             VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE) const
      -> Error;

  auto TransitionLayout(
      const GraphicsContext &context, VkImageLayout layout,
      VkPipelineStageFlags2 sourceStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
                                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VkPipelineStageFlags2 destinationStage =
          VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT |
          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      VkAccessFlags2 srcAccessMask = VK_ACCESS_2_NONE, // NOLINT
      VkAccessFlags2 dstAccessMask = VK_ACCESS_2_NONE,
      VkImageSubresourceRange range = {
          .levelCount = VK_REMAINING_MIP_LEVELS,
          .layerCount = VK_REMAINING_ARRAY_LAYERS}) const -> Error;

  VkExtent3D size{};

  // Indicates if this texture is a swapchain image, which should not be destroyed
  bool isSwapchainMemory = false;

  enum TextureType textureType = TextureType::ENUM_MAX;
};

struct Texture : Object, Identifiable {
  static auto Create(const GraphicsContext &context,
                     const TextureCreationInfo &info) -> Result<Ref<Texture>>;
  static auto Create(const GraphicsContext &context, Texture *parentTexture,
                     VkImageViewType viewType, VkImageSubresourceRange range)
      -> Result<Ref<Texture>>;

  static auto FromSwapchain(const GraphicsContext &context,
                            VkImage swapchainImage,
                            VkImageView swapchainImageView, VkFormat format,
                            uint32_t width, uint32_t height)
      -> Result<Ref<Texture>>;

  auto CopyTo(GraphicsContext &context, Texture *texture, VkBuffer buffer)
      -> Error;
  auto GenerateMipmaps(const GraphicsContext &context,
                       VkCommandBuffer commandBuffer) const -> Error;
  static auto FromFile(const GraphicsContext &context, const char *path,
                       VkImageUsageFlags usage = 0,
                       TextureMipmapOption mipmaps = {})
      -> Result<Ref<Texture>>;

  static auto
  FromMemory(const GraphicsContext &context, Image::ImageData &imageData,
             VkImageUsageFlags usage = 0, TextureMipmapOption mipmaps = {})
      -> Result<Ref<Texture>>;

  static auto FromMemory(const GraphicsContext &context,
                         const std::vector<Ref<Image::ImageData>> &slices,
                         TextureType type, VkImageUsageFlags usage = 0,
                         TextureMipmapOption mipmaps = {})
      -> Result<Ref<Texture>>;

  static auto FromMemory(const GraphicsContext &context,
                         const Image::CompressedImageData &compressedData,
                         VkImageUsageFlags usage = 0,
                         TextureMipmapOption mipmaps = {})
      -> Result<Ref<Texture>>;

  static auto GetDefault(const GraphicsContext &context, VkFormat format,
                         Graphics::TextureType textureType)
      -> Result<Ref<Graphics::Texture>>;

  std::mutex mutex;

  static std::atomic<VkDeviceSize> TotalAllocatedMemory;

  SamplerDescription samplerDescription{};

  VkImageView view = VK_NULL_HANDLE;
  VkSampler sampler = VK_NULL_HANDLE;

  uint32_t baseMipLevel{};
  uint32_t levelCount{1};
  uint32_t baseArrayLayer{};
  uint32_t layerCount{1};

  std::string debugName;

  // Flag to indicate if the sampler needs to be updated
  bool samplerDirty = false;

  // Safety flag to prevent double releases
  bool released = false;

  // Indicate if the texture has been destroyed, to avoid calling defer release multiple times
  bool isDestroyed = false;

  bool isView = false;

  std::shared_ptr<ImageMemory> imageMemory;

  auto UseAs(const GraphicsContext &context, TextureUsage newUsage,
             VkPipelineStageFlags2 stage,
             VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
             VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE)
      -> Error;

  auto
  UseAsAttachment(const GraphicsContext &context,
                  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                  VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE)
      -> Error;
  auto UseAsSampler(const GraphicsContext &context, VkPipelineStageFlags2 stage)
      -> Error;
  auto UseAsTransferSrc(const GraphicsContext &context) -> Error;
  auto UseAsTransferDst(const GraphicsContext &context) -> Error;
  auto UseAsStorage(const GraphicsContext &context, VkPipelineStageFlags2 stage)
      -> Error;
  auto UseAsPresentSrc(const GraphicsContext &context) -> Error;

  auto GetTimestamp() const -> uint64_t {
    return imageMemory->lastUsedTimestamp;
  }

  auto MarkUse() const -> void;

  auto CopyTo(const GraphicsContext &context, Texture &dstTexture,
              CopyRegion region) -> Error;

  auto CopyTo(const GraphicsContext &context, Buffer &dstBuffer,
              ToBufferCopyRegion region) -> Error;

  Texture() = default;
  Texture(const Texture &) = delete;
  auto operator=(const Texture &) -> Texture & = delete;
  Texture(Texture &&) noexcept = delete;
  auto operator=(Texture &&) noexcept -> Texture & = delete;
  ~Texture() override;

  auto SetFilter(VkFilter minFilter, VkFilter magFilter,
                 VkSamplerMipmapMode mipFilter) -> void;
  [[nodiscard]] auto GetFilter() const
      -> std::tuple<VkFilter, VkFilter, VkSamplerMipmapMode>;
  auto SetBorderColor(VkBorderColor borderColor) -> void;
  [[nodiscard]] auto GetBorderColor() const -> VkBorderColor;
  auto SetAnisotropy(float anisotropy) -> void;
  [[nodiscard]] auto GetAnisotropy() const -> float;
  auto SetWrapmode(VkSamplerAddressMode addressModeU,
                   VkSamplerAddressMode addressModeV,
                   VkSamplerAddressMode addressModeW) -> void;
  [[nodiscard]] auto GetWrap() const
      -> std::tuple<VkSamplerAddressMode, VkSamplerAddressMode,
                    VkSamplerAddressMode>;
  auto SetLodBias(float mipLodBias) -> void;
  [[nodiscard]] auto GetLodBias() const -> float;
  auto SetLodRange(float minLod, float maxLod) -> void;
  [[nodiscard]] auto GetLodRange() const -> std::tuple<float, float>;
  auto SetDepthCompare(bool enable, VkCompareOp compareOp) -> void;
  [[nodiscard]] auto GetDepthCompare() const -> std::tuple<bool, VkCompareOp>;
  [[nodiscard]] auto GetWidth() const -> uint32_t {
    auto baseWidth = imageMemory->size.width;
    return baseWidth >> baseMipLevel;
  };
  [[nodiscard]] auto GetHeight() const -> uint32_t {
    auto baseHeight = imageMemory->size.height;
    return baseHeight >> baseMipLevel;
  };
  [[nodiscard]] auto GetDepth() const -> uint32_t {
    auto baseDepth = imageMemory->size.depth;
    return baseDepth >> baseMipLevel;
  };
  [[nodiscard]] auto GetDimensions() const -> VkExtent2D {
    return {GetWidth(), GetHeight()};
  };
  [[nodiscard]] auto GetDimensions3D() const -> VkExtent3D {
    return {GetWidth(), GetHeight(), GetDepth()};
  };

  auto GetSampler(const GraphicsContext &context) -> VkSampler;
  // Copies data from a 1D/2D/3D region from the provided data to the texture
  // sourceSize is the dimensions of the underlying data
  // sourceOffset is the offset within the data to start copying from
  // target is the offset within the texture to copy to
  // targetSize is the size of the region to copy within the texture
  auto SetPixels(const GraphicsContext &context,
                 const std::span<const uint8_t> &data,
                 uint32_t mipLevel, // NOLINT
                 uint32_t arrayLayer, VkExtent3D sourceSize,
                 VkOffset3D sourceOffset, // NOLINT
                 VkOffset3D target, VkExtent3D targetSize) -> Error;
  auto SetPixels(const GraphicsContext &context, Image::ImageData &imageData,
                 uint32_t mipLevel = 0, uint32_t arrayLayer = 0) -> Error;
  [[nodiscard]] auto GetMipmapCount() const -> size_t { return levelCount; }

  [[nodiscard]] auto GetArrayLayerCount() const -> size_t { return layerCount; }

  [[nodiscard]] auto GetFormat() const -> VkFormat {
    return imageMemory->format;
  }
  [[nodiscard]] auto SupportsStorage() const -> bool {
    return (imageMemory->usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
  }
  [[nodiscard]] auto SupportsSampling() const -> bool {
    return (imageMemory->usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0;
  }
  [[nodiscard]] auto SupportsAttachment() const -> bool {
    return (imageMemory->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0 ||
           (imageMemory->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) !=
               0;
  }

  [[nodiscard]] auto IsDepthTexture() const -> bool {
    return Image::IsDepthTexture(imageMemory->format);
  }

  [[nodiscard]] auto IsStencilTexture() const -> bool {
    return Image::IsStencilTexture(imageMemory->format);
  }

  [[nodiscard]] auto IsDepthOrStencilTexture() const -> bool {
    return Image::IsDepthOrStencilTexture(imageMemory->format);
  }

  static auto GetType() -> Type const * { return &LuaTextureType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return GetType();
  }

  [[nodiscard]] auto GetDebugName() const -> std::string_view {
    if (!debugName.empty()) {
      return debugName;
    }

    return "Unnamed Texture";
  }

  auto operator==(const Texture &other) const -> bool {
    return view == other.view && imageMemory->image == other.imageMemory->image;
    // Two textures are considered equal if they reference the same image and view
  }

  auto operator!=(const Texture &other) const -> bool {
    return !(*this == other);
  }
};

} // namespace Graphics