#include "shaderCache.hpp"
#include "Modules/filesystem.hpp"
#include <cstdint>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <vector>

namespace Graphics {

// NOLINTBEGIN
auto ShaderCache::Serialize() const -> std::vector<uint8_t> {
  ShaderCacheHeader header{
      .moduleNameSize = modulename.size(),
      .spirvSize = spirv.size() * sizeof(uint32_t),
      .modtimeCount = modtimes.size(),
      .dependencyCount = dependencies.size(),
  };

  size_t size = sizeof(header) + header.moduleNameSize + header.spirvSize + header.modtimeCount * sizeof(uint64_t);

  for (const auto &dep : dependencies)
    size += sizeof(uint64_t) + dep.size();

  std::vector<uint8_t> data(size);

  uint8_t *output = data.data();

  auto write = [&](const void *src, size_t size) {
    memcpy(output, src, size);
    output += size;
  };

  write(&header, sizeof(header));
  write(modulename.data(), modulename.size());
  write(spirv.data(), spirv.size() * sizeof(uint32_t));
  write(modtimes.data(), modtimes.size() * sizeof(uint64_t));

  for (const auto &dep : dependencies) {
    uint64_t len = dep.size();
    write(&len, sizeof(len));
    write(dep.data(), dep.size());
  }

  return data;
}

auto ShaderCache::Deserialize(const std::span<const uint8_t> &data) -> ShaderCache {
  ShaderCache cache;

  const uint8_t *input = data.data();

  auto read = [&](void *dst, size_t size) {
    memcpy(dst, input, size);
    input += size;
  };

  ShaderCacheHeader header;
  read(&header, sizeof(header));

  cache.modulename.resize(header.moduleNameSize);
  read(cache.modulename.data(), header.moduleNameSize);

  cache.spirv.resize(header.spirvSize / sizeof(uint32_t));
  read(cache.spirv.data(), header.spirvSize);

  cache.modtimes.resize(header.modtimeCount);
  read(cache.modtimes.data(), header.modtimeCount * sizeof(int64_t));

  cache.dependencies.reserve(header.dependencyCount);

  for (uint64_t i = 0; i < header.dependencyCount; i++) {
    uint64_t len;
    read(&len, sizeof(len));

    cache.dependencies.emplace_back(reinterpret_cast<const char *>(input), len);

    input += len;
  }

  return cache;
}
// NOLINTEND

auto ShaderCacheManager::TryLoadModule(const std::string_view &modulename) -> Result<std::optional<ShaderCache>> {
  auto path = std::format("{}/{}", cachePath, Path::PlainText(modulename));

  if (!Filesystem::FileExists(path)) {
    return std::nullopt;
  }

  auto data = CHECK_RES(Filesystem::ReadFile(path));

  auto cache = ShaderCache::Deserialize(data);

  for (int i = 0; i < cache.dependencies.size(); i++) {
    const auto &dependency = cache.dependencies.at(i);
    if (Filesystem::GetFileInfo(dependency).modtime != cache.modtimes.at(i)) {
      return std::nullopt;
    }
  }

  return cache;
}

auto ShaderCacheManager::SaveModule(const ShaderCache &cache) -> Error {
  auto path = std::format("{}/{}", cachePath, Path::PlainText(cache.modulename));

  auto serialized = cache.Serialize();

  CHECK_ERR(Filesystem::WriteFile(path, serialized));

  return {};
}

} // namespace Graphics