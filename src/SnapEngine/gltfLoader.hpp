#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include <cstdint>
#include <flecs.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
namespace glTF {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<std::vector<std::uint8_t>> Buffers;
extern std::unordered_map<std::string, std::vector<std::uint8_t>> URICache;
extern std::mutex URICacheMutex;
extern std::vector<std::variant<Ref<Image::ImageData>, Ref<Image::CompressedImageData>>> ImageCache;
extern std::vector<Ref<struct Graphics::Texture>> TextureCache;
extern std::unordered_map<std::string, uint16_t> NameDuplicateCount;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto LoadGltfModel(Graphics::GraphicsContext &context, const std::string &path, flecs::world *world) -> Error;

} // namespace glTF