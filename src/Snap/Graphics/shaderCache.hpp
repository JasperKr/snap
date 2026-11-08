#pragma once

#include "Modules/error.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace Graphics {

struct ShaderCache {
  struct ShaderCacheHeader {
    uint64_t moduleNameSize;
    uint64_t spirvSize;
    uint64_t modtimeCount;
    uint64_t dependencyCount;
  } header;

  std::string modulename;
  std::vector<uint32_t> spirv;
  std::vector<int64_t> modtimes;
  std::vector<std::string> dependencies;

  [[nodiscard]] auto Serialize() const -> std::vector<uint8_t>;
  static auto Deserialize(const std::span<const uint8_t> &data) -> ShaderCache;
};

struct ShaderCacheManager {
  const char *cachePath = ".shaderCache";

  auto TryLoadModule(const std::string_view &modulename) -> Result<std::optional<ShaderCache>>;

  auto SaveModule(const ShaderCache &cache) -> Error;
};

}; // namespace Graphics