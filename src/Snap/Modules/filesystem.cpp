#include "filesystem.hpp"
#include "../external/physfs/src/physfs.h"
#include "Modules/bytedata.hpp"
#include "error.hpp"
#include <atomic>
#include <cctype>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Filesystem {
constexpr int PHYSFS_ERR_ERROR = 0;

std::atomic<bool> FilesystemInitialized{false}; // NOLINT

inline auto CheckIsInitialized() -> Error {
  return FilesystemInitialized.load(std::memory_order_relaxed) ? Error::Success() : Error::Create("Filesystem uninitialized");
}

inline auto GetErrorString() -> const char * { return PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()); }
inline auto GetErrorCode() -> uint32_t { return PHYSFS_getLastErrorCode(); }

inline auto GetError() -> Error {
  return Error::Create(std::format("Filesystem error occurred: {}", GetErrorString()), static_cast<ErrorCode>(GetErrorCode()));
}

auto GetConfig() -> Config & {
  static Config config = {};
  return config;
}

auto Init(const std::string &orgDir) -> Error {
  if (PHYSFS_isInit() != 0) {
    return Error::Createf("Filesystem already initialized: {}", GetErrorString());
  }

  if (PHYSFS_init(orgDir.c_str()) == 0) {
    return Error::Createf("Failed to initialize PhysFS: {}", GetErrorString());
  }

  PHYSFS_permitSymbolicLinks(0);

  FilesystemInitialized.store(true);

  return Error::Success();
}

auto SetWriteDirectory(const std::string &path) -> Error {
  if (PHYSFS_isInit() == 0) {
    return Error::Createf("Filesystem not initialized: {}", GetErrorString());
  }

  if (PHYSFS_setWriteDir(path.c_str()) == 0) {
    return Error::Createf("Failed to set write directory: {}", GetErrorString());
  }

  return Error::Success();
}

auto Deinit() -> Error {
  if (PHYSFS_isInit() == 0) {
    return Error::Createf("Filesystem not initialized: {}", GetErrorString());
  }

  PHYSFS_deinit();

  FilesystemInitialized.store(false);

  return Error::Success();
}

auto ReadFile(const std::string &path, int64_t readLength) -> Result<std::vector<unsigned char>> {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openRead(path.c_str());
  if (file == nullptr) {
    return Error::Unexpected("Failed to open file: " + path);
  }

  const PHYSFS_sint64 fileLength = PHYSFS_fileLength(file);
  PHYSFS_sint64 len = 0;

  if (fileLength <= 0) {
    PHYSFS_close(file);
    return Error::Unexpected("Invalid file length");
  }

  if (readLength < 0) {
    PHYSFS_close(file);
    return Error::Unexpected("Invalid read length");
  }

  if (readLength > fileLength && readLength != INT64_MAX) {
    PHYSFS_close(file);
    return Error::Unexpected("Read length exceeds file length");
  }

  if (readLength == INT64_MAX) {
    len = fileLength; // Checked to be > 0 above
  } else {
    len = readLength; // Checked to be valid above
  }

  std::vector<unsigned char> data(len);

  const auto read = PHYSFS_readBytes(file, data.data(), len);

  if (PHYSFS_close(file) == PHYSFS_ERR_ERROR) {
    return Error::Unexpected("Failed to close file");
  }

  if (read != len) {
    return Error::Unexpected("Failed to read entire file");
  }

  return data;
}

// Useful for loading binary data into Bytedata objects
// Instead of stack allocating a vector and copying it over.
auto ReadFileToBytedata(const std::string &path, int64_t readLength) -> Result<Ref<Data::ByteData>> {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openRead(path.c_str());
  if (file == nullptr) {
    return Error::Unexpected("Failed to open file: " + path);
  }

  const PHYSFS_sint64 fileLength = PHYSFS_fileLength(file);
  PHYSFS_sint64 len = 0;

  if (fileLength <= 0) {
    PHYSFS_close(file);
    return Error::Unexpected("Invalid file length");
  }

  if (readLength < 0) {
    PHYSFS_close(file);
    return Error::Unexpected("Invalid read length");
  }

  if (readLength > fileLength && readLength != INT64_MAX) {
    PHYSFS_close(file);
    return Error::Unexpected("Read length exceeds file length");
  }

  if (readLength == INT64_MAX) {
    len = fileLength; // Checked to be > 0 above
  } else {
    len = readLength; // Checked to be valid above
  }

  auto data = Ref<Data::ByteData>::Make((size_t)len);

  const auto read = PHYSFS_readBytes(file, data->GetData(), len);
  int error = PHYSFS_close(file);

  if (error == PHYSFS_ERR_ERROR) {
    return Error::Unexpected("Failed to close file");
  }

  if (read != len) {
    return Error::Unexpected("Failed to read entire file");
  }

  return data;
}

auto ReadTextFile(const std::string &path, int64_t readLength) -> Result<std::string> {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openRead(path.c_str());
  if (file == nullptr) {
    return Error::Unexpected("Failed to open file: " + path);
  }

  const PHYSFS_sint64 fileLength = PHYSFS_fileLength(file);
  if (fileLength <= 0) {
    PHYSFS_close(file);
    return Error::Unexpected("Invalid file length");
  }

  PHYSFS_sint64 len = 0;
  if (readLength < 0) {
    PHYSFS_close(file);
    return Error::Unexpected("Invalid read length");
  }

  if (readLength > fileLength && readLength != INT64_MAX) {
    PHYSFS_close(file);
    return Error::Unexpected("Read length exceeds file length");
  }

  if (readLength == INT64_MAX) {
    len = fileLength; // Checked to be > 0 above
  } else {
    len = readLength; // Checked to be valid above
  }

  std::string data((size_t)len, '\0');

  const auto read = PHYSFS_readBytes(file, data.data(), len);
  int error = PHYSFS_close(file);

  if (error == PHYSFS_ERR_ERROR) {
    return Error::Unexpected("Failed to close file");
  }

  if (read != len) {
    return Error::Unexpected("Failed to read entire file");
  }

  return data;
}

auto AppendFile(const std::string &path, std::span<const uint8_t> data) -> Error {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openAppend(path.c_str());
  if (file == nullptr) {
    return Error::Createf("Failed to open file for appending: {}", GetErrorString());
  }

  PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.data(), data.size());
  int error = PHYSFS_close(file);

  if (error == PHYSFS_ERR_ERROR) {
    return Error::Createf("Failed to close file: {}", GetErrorString());
  }

  if (written < 0 || written != data.size()) {
    return Error::Createf("Failed to write all data to file: {}", GetErrorString());
  }

  return Error::Success();
}

auto AppendFile(const std::string &path, std::string_view data) -> Error {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openAppend(path.c_str());
  if (file == nullptr) {
    return Error::Createf("Failed to open file for appending: {}", GetErrorString());
  }

  PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.data(), data.size());
  int error = PHYSFS_close(file);

  if (error == PHYSFS_ERR_ERROR) {
    return Error::Createf("Failed to close file: {}", GetErrorString());
  }

  if (written < 0 || written != data.size()) {
    return Error::Createf("Failed to write all data to file: {}", GetErrorString());
  }

  return Error::Success();
}

auto WriteFile(const std::string &path, std::span<const uint8_t> data) -> Error {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openWrite(path.c_str());
  if (file == nullptr) {
    return Error::Createf("Failed to open file '{}' for writing: {}", path, GetErrorString());
  }

  PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.data(), data.size());
  int error = PHYSFS_close(file);

  if (error == PHYSFS_ERR_ERROR) {
    return Error::Createf("Failed to close file: {}", GetErrorString());
  }

  if (written < 0 || written != data.size()) {
    return Error::Createf("Failed to write all data to file: {}", GetErrorString());
  }

  return Error::Success();
}

auto WriteFile(const std::string &path, std::string_view data) -> Error {
  CHECK_ERR(CheckIsInitialized());

  PHYSFS_File *file = PHYSFS_openWrite(path.c_str());
  if (file == nullptr) {
    return Error::Createf("Failed to open file for writing: {}", GetErrorString());
  }

  PHYSFS_sint64 written = PHYSFS_writeBytes(file, data.data(), data.size());
  int error = PHYSFS_close(file);

  if (error == PHYSFS_ERR_ERROR) {
    return Error::Createf("Failed to close file: {}", GetErrorString());
  }

  if (written < 0 || written != data.size()) {
    return Error::Createf("Failed to write all data to file: {}", GetErrorString());
  }

  return Error::Success();
}

auto FileExists(const std::string &path) -> bool {
  if (Error::IsError(CheckIsInitialized())) {
    return false;
  }

  return PHYSFS_exists(path.c_str()) != 0;
}

auto AddToSearchPath(const std::string &path, bool appendToPath) -> Error {
  CHECK_ERR(CheckIsInitialized());

  if (PHYSFS_mount(path.c_str(), nullptr, appendToPath ? 1 : 0) == 0) {
    return GetError();
  }
  return Error::Success();
}

auto RemoveFromSearchPath(const std::string &path) -> Error {
  CHECK_ERR(CheckIsInitialized());

  if (PHYSFS_unmount(path.c_str()) == 0) {
    return Error::Createf("Failed to remove path from search path: {}", GetErrorString());
  }
  return Error::Success();
}

auto GetRealPath(const std::string &path) -> Result<std::string> {
  CHECK_ERR(CheckIsInitialized());

  const char *realPath = PHYSFS_getRealDir(path.c_str());
  if (realPath == nullptr) {
    return Error::Unexpected("Failed to get real path of: " + path);
  }

  return std::string(realPath);
}

auto ListFiles(const std::string &path) -> Result<std::vector<std::string>> {
  CHECK_ERR(CheckIsInitialized());

  auto *fileList = PHYSFS_enumerateFiles(path.c_str());
  if (fileList == nullptr) {
    return Error::Unexpected("Failed to list files");
  }

  std::vector<std::string> files;
  // NOLINTNEXTLINE (clang-diagnostic-pointer-arith)
  for (char **i = fileList; *i != nullptr; i++) {
    files.emplace_back(*i);
  }

  PHYSFS_freeList(static_cast<void *>(fileList));

  return files;
}

/*
Error Mount(std::string path, const char *mountPoint, bool appendToPath);
Error Unmount(std::string path);
*/

auto Mount(const std::string &path, const std::string &mountPoint, bool appendToPath) -> Error {
  CHECK_ERR(CheckIsInitialized());

  if (PHYSFS_mount(path.c_str(), mountPoint.c_str(), appendToPath ? 1 : 0) == 0) {
    return Error::Createf("Failed to mount path: {}", GetErrorString());
  }
  return Error::Success();
}

auto Unmount(const std::string &path) -> Error {
  CHECK_ERR(CheckIsInitialized());

  if (PHYSFS_unmount(path.c_str()) == 0) {
    return Error::Createf("Failed to unmount path: {}", GetErrorString());
  }
  return Error::Success();
}

auto GetFileInfo(const std::string &path) -> PHYSFS_Stat {
  PHYSFS_Stat stat;
  if (PHYSFS_stat(path.c_str(), &stat) == 0) {
    return {};
  }
  return stat;
}

inline auto GetSourceDirectoryStorage() -> std::string & {
  static std::string sourceDirectory;
  return sourceDirectory;
}

auto SetSourceDirectory(const std::string &path) -> Error {
  if (GetSourceDirectoryStorage().empty()) {
    GetSourceDirectoryStorage() = path;
    return Error::Success();
  }

  return Error::Createf("Source directory already set: {}", GetErrorString());
}

auto GetSaveDirectory() -> std::string {
  static std::string identity;

  if (identity.empty()) {
    identity = std::string(PHYSFS_getPrefDir("snap", GetConfig().identity.c_str()));
  }

  return identity;
}

auto GetSourceDirectory() -> std::string { return GetSourceDirectoryStorage(); }

auto GetSourceBaseDirectory() -> std::string {
  static std::string source;

  if (source.empty()) {
    source = std::string(PHYSFS_getBaseDir());
  }

  return source;
}

#ifdef CreateDirectory
#undef CreateDirectory
#endif

auto CreateDirectory(const std::string &path) -> Error {
  if (PHYSFS_mkdir(path.c_str()) == 0) {
    return Error::Createf("Failed to create directory '{}': {}", path, GetErrorString());
  }
  return Error::Success();
}
} // namespace Filesystem

namespace Path {

// Returns the extension of the file
// file.txt -> txt
auto Extension(const std::string &path) -> std::string {
  size_t dotPos = path.find_last_of('.');
  if (dotPos == std::string::npos) {
    return "";
  }
  return path.substr(dotPos + 1);
}

// Returns the filename from a path
// /path/to/file.txt -> file.txt
auto Filename(const std::string &path) -> std::string {
  size_t slashPos = path.find_last_of("/\\");
  if (slashPos == std::string::npos) {
    return path;
  }
  return path.substr(slashPos + 1);
}

// Returns the directory from a path
// /path/to/file.txt -> /path/to/
auto Directory(const std::string &path) -> std::string {
  size_t slashPos = path.find_last_of("/\\");
  if (slashPos == std::string::npos) {
    return "";
  }
  return path.substr(0, slashPos + 1);
}

inline auto SanitizeSlashes(const std::string &path) -> std::string {
  std::string sanitized;
  sanitized.reserve(path.size());

  bool lastWasSlash = false;
  for (char character : path) {
    if (character == '\\' || character == '/') {
      if (!lastWasSlash) {
        sanitized += '/';
        lastWasSlash = true;
      }
    } else {
      sanitized += character;
      lastWasSlash = false;
    }
  }

  return sanitized;
}

// Sanitizes a path by replacing backslashes with forward slashes
// and removing redundant slashes and applying .. with preceding directories
// C:\path\to\\file.txt -> C:/path/to/file.txt
auto Sanitize(const std::string &path) -> std::string {
  auto sanitized = SanitizeSlashes(path);
  // Handle .. in paths

  std::vector<std::string> parts;
  size_t start = 0;
  size_t end = sanitized.find('/');

  // Loop through each part of the path
  while (end != std::string::npos) {
    std::string part = sanitized.substr(start, end - start);
    if (part == "..") { // Remove previous part if ..
      if (!parts.empty()) {
        // if previous part is also .., keep it
        if (parts.back() == "..") {
          parts.emplace_back(part);
        } else {
          parts.pop_back();
        }
      }
    } else if (part != "." || start == 0) { // remove ./ parts if not at start
      parts.emplace_back(part);
    }
    start = end + 1;
    end = sanitized.find('/', start);
  }

  std::string lastPart = sanitized.substr(start);
  if (lastPart == "..") {
    if (!parts.empty()) {
      if (parts.back() == "..") {
        parts.emplace_back(lastPart);
      } else {
        parts.pop_back();
      }
    }
  } else if (lastPart != ".") {
    parts.emplace_back(lastPart);
  }

  sanitized.clear();
  for (size_t i = 0; i < parts.size(); ++i) {
    sanitized += parts[i];
    if (i < parts.size() - 1) {
      sanitized += '/';
    }
  }

  return sanitized;
}

inline auto Join(std::string_view str) -> std::string { return Sanitize(std::string(str)); }

auto Join(const std::string &base, const std::string &append) -> std::string {
  if (base.empty()) {
    return append;
  }
  if (append.empty()) {
    return base;
  }

  if (base.back() == '/') {
    if (append.front() == '/') {
      return Sanitize(base + append.substr(1));
    }
    return Sanitize(base + append);
  }

  if (append.front() == '/') {
    return Sanitize(base + append);
  }

  return Sanitize(base + '/' + append);
}

auto Join(const std::vector<std::string> &paths) -> std::string {
  if (paths.empty()) {
    return "";
  }

  std::string result = paths[0];
  for (size_t i = 1; i < paths.size(); ++i) {
    result = Join(result, paths[i]);
  }
  return result;
}

auto Join(const char *base, const char *append) -> std::string { return Join(std::string(base), std::string(append)); }

// test/my.file.txt -> test_my_file_txt
auto PlainText(const std::string_view &filename) -> std::string {
  std::string result;
  result.reserve(filename.size());

  for (unsigned char character : filename) {
    result += (std::isalnum(character) != 0) ? static_cast<char>(character) : '_';
  }

  return result;
}

} // namespace Path