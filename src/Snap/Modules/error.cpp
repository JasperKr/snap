#include "error.hpp"

#define LOG_ERRORS

#include "Modules/console.hpp"
#include "Modules/filesystem.hpp"
#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <print>
#include <sstream>
#include <unordered_map>

#include <string>

#include "tl/expected.hpp"
#include <vulkan/vulkan.h>

#if __cpp_lib_stacktrace
#include <vector>
#define STD_STACKTRACE_SUPPORTED 1
#include <stacktrace>
#endif

#ifndef STD_STACKTRACE_SUPPORTED
#if defined(_WIN32)
#include <windows.h>

#include <dbghelp.h>
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)
#include <backtrace.h>
#include <cstdlib>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <libunwind-x86_64.h>
#include <libunwind.h>
#endif
#endif

inline auto CleanupTracebackLine(const std::string &line) -> std::string {
  // Cut off each line at +0x
  size_t plusPos = line.find("+0x");
  std::string sanitizedLine;

  if (plusPos != std::string::npos) {
    sanitizedLine = line.substr(0, plusPos) + "\n";
  } else {
    sanitizedLine = line + "\n";
  }

  // Replace backslashes with forward slashes
  for (char &atIndex : sanitizedLine) {
    if (atIndex == '\\') {
      atIndex = '/';
    }
  }

  // Split before and after "snap!"
  const std::string splitKeyword = "snap!";
  size_t SnapPos = sanitizedLine.find(splitKeyword);
  std::string inFunction = "Unknown";

  if (SnapPos != std::string::npos) {
    auto inFunctionPos = SnapPos + splitKeyword.length();
    auto inFunctionCount =
        sanitizedLine.length() - inFunctionPos - 1; // -1 for newline
    inFunction =
        sanitizedLine.substr(SnapPos + splitKeyword.length(), inFunctionCount);
    sanitizedLine = sanitizedLine.substr(0, SnapPos)
                        .append("in function \'")
                        .append(inFunction)
                        .append("\'\n");
  }

  // Sanitize paths by removing everything before src/
  size_t srcPos = sanitizedLine.find("src/");
  if (srcPos != std::string::npos) {
    sanitizedLine = sanitizedLine.substr(srcPos);
  }

  // Remove (line) and change to :line
  size_t linePos = sanitizedLine.find('(');
  if (linePos != std::string::npos) {
    size_t endLinePos = sanitizedLine.find(')', linePos);
    if (endLinePos != std::string::npos) {
      std::string lineNumber =
          sanitizedLine.substr(linePos + 1, endLinePos - linePos - 1);

      auto stringAfterLineNumber = sanitizedLine.substr(endLinePos + 1);
      sanitizedLine = sanitizedLine.substr(0, linePos)
                          .append(":")
                          .append(lineNumber)
                          .append(stringAfterLineNumber);
    }
  }

  auto source = Filesystem::GetSourceDirectory();
  if (!source.empty()) {
    size_t sourcePos = sanitizedLine.find(source);
    if (sourcePos != std::string::npos) {
      sanitizedLine = sanitizedLine.substr(0, sourcePos) +
                      sanitizedLine.substr(sourcePos + source.length());
    }
  }

  return "\t" + sanitizedLine;
}

static auto Demangle(const char *name) -> std::string {
  int status = 0;
  char *demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
  std::string result = (status == 0) ? demangled : name;
  free(demangled); // NOLINT
  return result;
}

static void callback(void * /*unused*/, uintptr_t _pc, const char *filename,
                     int lineno, const char *function) {
  std::println("{}:{} {}", filename, lineno, function);
}

static auto is_noisy(const char *file) -> bool {
  if (file == nullptr) {
    return true;
  }

  std::string_view name(file);
  return name.find("bits/") != std::string_view::npos ||
         name.find("include/c++") != std::string_view::npos ||
         name.find("__") != std::string_view::npos;
};

[[gnu::cold]] // NOLINTNEXTLINE
auto GetStackTrace(ErrorLevel level) -> std::string {
  const int MaxStackDepth = 64;

#ifndef STD_STACKTRACE_SUPPORTED

#if defined(_WIN32)
  const int MaxSymbolLength = 256;
  std::array<void *, MaxStackDepth> stack = {};

  unsigned short frames =
      CaptureStackBackTrace(2 + level, MaxStackDepth, stack.data(), nullptr);
  std::string trace;
  HANDLE process = GetCurrentProcess();

  // NOLINTNEXTLINE
  auto *symbol = // NOLINTNEXTLINE
      (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + MaxSymbolLength, 1);
  symbol->MaxNameLen = MaxSymbolLength - 1;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  for (unsigned short i = 0; i < frames; i++) {
    auto address = (DWORD64)(stack.at(i)); // NOLINT
    if (SymFromAddr(process, address, nullptr, symbol) != 0) {
      trace += "\tin function: ";
      trace += symbol->Name; // NOLINT
      trace += "\n";
    } else {
      trace += std::to_string(i) + ": [symbol lookup failed] - " +
               std::to_string(address) + "\n";
    }
  }

  return trace;
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)

  static backtrace_state *state =
      backtrace_create_state("/proc/self/exe", 0, nullptr, nullptr);

  struct TraceCtx {
    std::string *out;
  };

  std::string trace;

  unw_cursor_t cursor;
  unw_context_t context;

  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  TraceCtx traceCtx{.out = &trace};

  // NOLINTBEGIN
  while (unw_step(&cursor) > 0) {
    unw_word_t ip;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);

    // char name[256];
    // unw_get_proc_name(&cursor, name, sizeof(name), nullptr);
    // std::string demangledName = Demangle(name);
    // trace += "\t" + demangledName + "\n";

    backtrace_pcinfo(
        state, ip,
        [](void *data, uintptr_t, const char *filename, int lineno,
           const char *function) {
          if (is_noisy(filename)) {
            return 0; // Skip noisy frames
          }

          std::string demangledFunction =
              Demangle(function == nullptr ? "??" : function);

          // May be namespace::namespace::struct::function(namespace::namespace param)
          // We want to remove the parameters and just keep the function name
          size_t parenPos = demangledFunction.find('(');
          if (parenPos != std::string::npos) {
            demangledFunction = demangledFunction.substr(0, parenPos);
          }

          std::string line =
              "\t" +
              ColorText(std::string(filename == nullptr ? "??" : filename) +
                            ":" + std::to_string(lineno),
                        ConsoleColor::Cyan) +
              " in function " +
              ColorText(demangledFunction, ConsoleColor::Green) + "\n";

          auto *ctx = static_cast<TraceCtx *>(data);

          auto source = std::filesystem::current_path().string() + "/";
          if (!source.empty()) {
            size_t sourcePos = line.find(source);
            if (sourcePos != std::string::npos) {
              line = line.substr(0, sourcePos) +
                     line.substr(sourcePos + source.length());
            }
          }

          constexpr std::array<const char *, 11> blacklisted = {
              "expected.hpp", "error.hpp", "error.cpp", "thread.cpp",
              "thread.hpp",   "loop.cpp",  "loop.hpp",  "wrap.cpp",
              "wrap.hpp",     "main.cpp",  "main.hpp"};

          for (const auto &blacklistedFile : blacklisted) {
            size_t blacklistedPos = line.find(blacklistedFile);
            if (blacklistedPos != std::string::npos) {
              return 0; // Skip blacklisted files
            }
          }

          ctx->out->append(line);

          return 0;
        },
        nullptr, &traceCtx);
  }
  // NOLINTEND

  return trace;

#endif
#else
  std::string trace;
  std::stacktrace stacktrace =
      std::stacktrace::current(2 + level, MaxStackDepth);
  trace = std::to_string(stacktrace);

  std::string keyword = "!main";
  size_t pos = trace.find(keyword);
  if (pos != std::string::npos) {
    size_t endOfLine = trace.find('\n', pos);
    if (endOfLine != std::string::npos) {
      trace = trace.substr(0, endOfLine);
    }
  }

  std::vector<std::string> lines;
  size_t start = 0;
  size_t end = trace.find('\n');

  while (end != std::string::npos) {
    lines.push_back(trace.substr(start, end - start));
    start = end + 1;
    end = trace.find('\n', start);
  }

  std::string sanitizedTrace;

  for (const auto &line : lines) {
    sanitizedTrace += CleanupTracebackLine(line);
  }

  return sanitizedTrace;
#endif
  return "Unable to get stack trace on this platform.";
}

inline auto VkResultToString(ErrorCode result) -> std::string_view {
  switch (result) {
  case (VK_SUCCESS):
    return "VK_SUCCESS";
  case (VK_NOT_READY):
    return "VK_NOT_READY";
  case (VK_TIMEOUT):
    return "VK_TIMEOUT";
  case (VK_EVENT_SET):
    return "VK_EVENT_SET";
  case (VK_EVENT_RESET):
    return "VK_EVENT_RESET";
  case (VK_INCOMPLETE):
    return "VK_INCOMPLETE";
  case (VK_ERROR_OUT_OF_HOST_MEMORY):
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case (VK_ERROR_INITIALIZATION_FAILED):
    return "VK_ERROR_INITIALIZATION_FAILED";
  case (VK_ERROR_DEVICE_LOST):
    return "VK_ERROR_DEVICE_LOST";
  case (VK_ERROR_MEMORY_MAP_FAILED):
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case (VK_ERROR_LAYER_NOT_PRESENT):
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case (VK_ERROR_EXTENSION_NOT_PRESENT):
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case (VK_ERROR_FEATURE_NOT_PRESENT):
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case (VK_ERROR_INCOMPATIBLE_DRIVER):
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case (VK_ERROR_TOO_MANY_OBJECTS):
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case (VK_ERROR_FORMAT_NOT_SUPPORTED):
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case (VK_ERROR_FRAGMENTED_POOL):
    return "VK_ERROR_FRAGMENTED_POOL";
  case (VK_ERROR_UNKNOWN):
    return "VK_ERROR_UNKNOWN";
  case (VK_ERROR_VALIDATION_FAILED):
    return "VK_ERROR_VALIDATION_FAILED";
  case (VK_ERROR_OUT_OF_POOL_MEMORY):
    return "VK_ERROR_OUT_OF_POOL_MEMORY";
  case (VK_ERROR_INVALID_EXTERNAL_HANDLE):
    return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
  case (VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS):
    return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
  case (VK_ERROR_FRAGMENTATION):
    return "VK_ERROR_FRAGMENTATION";
  case (VK_PIPELINE_COMPILE_REQUIRED):
    return "VK_PIPELINE_COMPILE_REQUIRED";
  case (VK_ERROR_NOT_PERMITTED):
    return "VK_ERROR_NOT_PERMITTED";
  case (VK_ERROR_SURFACE_LOST_KHR):
    return "VK_ERROR_SURFACE_LOST_KHR";
  case (VK_ERROR_NATIVE_WINDOW_IN_USE_KHR):
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  case (VK_SUBOPTIMAL_KHR):
    return "VK_SUBOPTIMAL_KHR";
  case (VK_ERROR_OUT_OF_DATE_KHR):
    return "VK_ERROR_OUT_OF_DATE_KHR";
  case (VK_ERROR_INCOMPATIBLE_DISPLAY_KHR):
    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
  case (VK_ERROR_INVALID_SHADER_NV):
    return "VK_ERROR_INVALID_SHADER_NV";
  case (VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR):
    return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
  case (VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR):
    return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
  case (VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR):
    return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
  case (VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR):
    return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
  case (VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR):
    return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
  case (VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR):
    return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
  case (VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT):
    return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
  // case (VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT):
  //   return "VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT";
  case (VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT):
    return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
  case (VK_THREAD_IDLE_KHR):
    return "VK_THREAD_IDLE_KHR";
  case (VK_THREAD_DONE_KHR):
    return "VK_THREAD_DONE_KHR";
  case (VK_OPERATION_DEFERRED_KHR):
    return "VK_OPERATION_DEFERRED_KHR";
  case (VK_OPERATION_NOT_DEFERRED_KHR):
    return "VK_OPERATION_NOT_DEFERRED_KHR";
  case (VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR):
    return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
  case (VK_ERROR_COMPRESSION_EXHAUSTED_EXT):
    return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
  case (VK_INCOMPATIBLE_SHADER_BINARY_EXT):
    return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
  case (VK_PIPELINE_BINARY_MISSING_KHR):
    return "VK_PIPELINE_BINARY_MISSING_KHR";
  case (VK_ERROR_NOT_ENOUGH_SPACE_KHR):
    return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
  case (VK_RESULT_MAX_ENUM):
    return "VK_RESULT_MAX_ENUM";
  default:
    return "UNKNOWN_VK_RESULT";
  }
}

const std::unordered_map<SlangResult, std::string> SlangResultToStringMap = {
    {SLANG_OK, "SLANG_OK"},                                 // NOLINT
    {SLANG_FAIL, "SLANG_FAIL"},                             // NOLINT
    {SLANG_E_NOT_IMPLEMENTED, "SLANG_E_NOT_IMPLEMENTED"},   // NOLINT
    {SLANG_E_OUT_OF_MEMORY, "SLANG_E_OUT_OF_MEMORY"},       // NOLINT
    {SLANG_E_INVALID_ARG, "SLANG_E_INVALID_ARG"},           // NOLINT
    {SLANG_E_BUFFER_TOO_SMALL, "SLANG_E_BUFFER_TOO_SMALL"}, // NOLINT
    {SLANG_E_NOT_FOUND, "SLANG_E_NOT_FOUND"},               // NOLINT
};

[[nodiscard]] auto Error::ToString() const -> std::string {
  std::ostringstream oss;
  if (code >= 0) {
    oss << "Success: " << message;
    return oss.str();
  }
  oss << message << "\n(code " << code << ")";
  if (!backtrace.empty()) {
    oss << "\nBacktrace:\n" << backtrace;
  }
  return oss.str();
}

auto Error::SetupTraceback() -> void {
#ifndef STD_STACKTRACE_SUPPORTED
#if defined(_WIN32)
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
  SymInitialize(GetCurrentProcess(), nullptr, TRUE);
#endif
#endif
}

auto Error::Create(const std::string &message, ErrorCode code, ErrorLevel level)
    -> Error {

  if (code >= 0) {
    return Error(message, "", code);
  }

  Error err = Error(message, GetStackTrace(level), code);
#if defined(LOG_ERRORS)
  if (code < 0) {
    PrintError("{}", err.ToString());
  }
#endif
  return err;
}

auto Error::Create(const std::string_view &message, ErrorCode code,
                   ErrorLevel level) -> Error {
  return Create(std::string(message), code, level);
}

auto Error::Create(const char *message, ErrorCode code, ErrorLevel level)
    -> Error {
  return Create(std::string(message), code, level);
}

auto Error::Success() -> Error {
  static const Error success = Error();
  return success;
}

auto Error::IsError(const Error &error) -> bool { return error.code < 0; }
[[nodiscard]] auto Error::IsError() const -> bool { return code < 0; }
auto Error::IsError(const tl::expected<void, Error> &expected) -> bool {
  return !expected.has_value();
}

auto Error::IsError(const SlangResult result) -> bool {
  return SLANG_FAILED(result);
}
auto Error::IsSuccess(const Error &error) -> bool { return error.code >= 0; }
[[nodiscard]] auto Error::IsSuccess() const -> bool { return code >= 0; }

auto Error::Create(VkResult result) -> Error {
  if (result == VK_SUCCESS) {
    return Success();
  }

  return Create(VkResultToString(result), result, 1);
}

// NOLINTNEXTLINE
auto Error::Create(SlangResult result, ErrorLevel level) -> Error {
  if (SLANG_SUCCEEDED(result)) {
    return Success();
  }

  auto len = strlen(slang::getLastInternalErrorMessage());
  if (len > 0) {
    return Create(slang::getLastInternalErrorMessage(),
                  static_cast<ErrorCode>(result), level + 1);
  }

  /*
  Severity | Facility | Code
  ---------|----------|-----
  31       |    30-16 | 15-0

  Severity - 1 fail, 0 is success - as SlangResult is signed 32 bits, means
  negative number indicates failure. Facility is where the error originated
  from. Code is the code specific to the facility.

  Result codes have the following styles,
  1) SLANG_name
  2) SLANG_s_f_name
  3) SLANG_s_name

  where s is S for success, E for error
  f is the short version of the facility name

  Style 1 is reserved for SLANG_OK and SLANG_FAIL as they are so commonly used.

  It is acceptable to expand 'f' to a longer name to differentiate a name or
  drop if unique without it. ie for a facility 'DRIVER' it might make sense to
  have an error of the form SLANG_E_DRIVER_OUT_OF_MEMORY
  */

  // Check if the result is in the predefined map
  auto errStrIterator = SlangResultToStringMap.find(result);
  if (errStrIterator != SlangResultToStringMap.end()) {
    return Create(errStrIterator->second, static_cast<ErrorCode>(result),
                  level + 1);
  }

  auto errorBit = (static_cast<ErrorLevel>(result) >> 31U) & 0x1U; // NOLINT
  auto facilityBits = SLANG_GET_RESULT_FACILITY(result);           // NOLINT
  auto codeBits = SLANG_GET_RESULT_CODE(result);                   // NOLINT

  std::ostringstream oss;
  oss << "SlangResult Error - Severity: "
      << (errorBit == 1U ? "Error" : "Success")
      << ", Facility: " << facilityBits << ", Code: " << codeBits;
  return Create(oss.str(), static_cast<ErrorCode>(result), level + 1);
}

auto Error::Create(Slang::ComPtr<slang::IBlob> &diagnosticsBlob,
                   ErrorLevel level) -> Error {
  if (diagnosticsBlob.readRef() == nullptr) {
    return Success();
  }

  // NOLINTNEXTLINE
  return Create((const char *)diagnosticsBlob->getBufferPointer(), -1,
                level + 1);
}

auto Error::Unexpected(const std::string &message, ErrorCode code)
    -> tl::unexpected<Error> {
  return tl::unexpected<Error>(Create(message, code, 1));
}

auto Error::Unexpected(VkResult result) -> tl::unexpected<Error> {
  return tl::unexpected<Error>(Create(result));
}

[[nodiscard]] auto Error::AsUnexpected() const -> tl::unexpected<Error> {
  return tl::unexpected<Error>(*this);
};
