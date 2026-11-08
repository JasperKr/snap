#pragma once

#include "Modules/bytedata.hpp"
#include "error.hpp"
#include "physfs.h"
#include <atomic>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace Filesystem {
struct Config {
  std::string identity; // Application identity for save dirs
};

extern std::atomic<bool> FilesystemInitialized; // NOLINT

auto GetConfig() -> Config &;

auto Init(const std::string &orgDir) -> Error;
auto SetWriteDirectory(const std::string &path) -> Error;
auto Deinit() -> Error;
auto ReadFile(const std::string &path, int64_t readLength = INT64_MAX) -> Result<std::vector<unsigned char>>;
// Useful for loading binary data into Bytedata objects
// Instead of stack allocating a vector and copying it over.
auto ReadFileToBytedata(const std::string &path, int64_t readLength = INT64_MAX) -> Result<Ref<Data::ByteData>>;
auto ReadTextFile(const std::string &path, int64_t readLength = INT64_MAX) -> Result<std::string>;
auto AppendFile(const std::string &path, std::span<const uint8_t> data) -> Error;
auto AppendFile(const std::string &path, std::string_view data) -> Error;
auto WriteFile(const std::string &path, std::span<const uint8_t> data) -> Error;
auto WriteFile(const std::string &path, std::string_view data) -> Error;
auto FileExists(const std::string &path) -> bool;
auto GetFileInfo(const std::string &path) -> PHYSFS_Stat;
auto AddToSearchPath(const std::string &path, bool appendToPath) -> Error;
auto RemoveFromSearchPath(const std::string &path) -> Error;
auto Mount(const std::string &path, const std::string &mountPoint, bool appendToPath) -> Error;
auto Unmount(const std::string &path) -> Error;
auto GetRealPath(const std::string &path) -> Result<std::string>;
auto ListFiles(const std::string &path) -> Result<std::vector<std::string>>;

auto GetErrorCode() -> uint32_t;
auto GetError() -> Error;

auto SetSourceDirectory(const std::string &path) -> Error;

auto GetSaveDirectory() -> std::string;
auto GetSourceDirectory() -> std::string;
auto GetSourceBaseDirectory() -> std::string;

#ifdef CreateDirectory
#undef CreateDirectory
#endif

auto CreateDirectory(const std::string &path) -> Error;

} // namespace Filesystem

namespace Path {
// Returns the filename from a path
// /path/to/file.txt -> file.txt
auto Filename(const std::string &path) -> std::string;
// Returns the extension of the file
// file.txt -> txt
auto Extension(const std::string &path) -> std::string;
// Returns the directory from a path
// /path/to/file.txt -> /path/to/
auto Directory(const std::string &path) -> std::string;

auto Join(const std::string &base, const std::string &append) -> std::string;
auto Sanitize(const std::string &path) -> std::string;

template <typename... Strings> auto Join(std::string_view first, const Strings &...rest) -> std::string {
  if constexpr (sizeof...(rest) == 0) {
    return Sanitize(std::string(first));
  } else {
    auto combined = Join(rest...);
    return Join(std::string(first), combined);
  }
}

auto Join(const std::vector<std::string> &paths) -> std::string;
auto Join(const char *base, const char *append) -> std::string;
auto PlainText(const std::string_view &filename) -> std::string;

} // namespace Path