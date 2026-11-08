#pragma once

#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

enum class LogLevel : uint8_t {
  Debug,   // Detailed debug information
  Info,    // General information about application
  Warning, // Indications of potential issues
  Error,   // Serious issues that need attention
  Fatal    // Critical errors causing application termination
};

#ifdef NDEBUG
// NOLINTNEXTLINE
inline LogLevel CurrentLogLevel = LogLevel::Warning;
#else
// NOLINTNEXTLINE
inline LogLevel CurrentLogLevel = LogLevel::Debug;
#endif

inline auto SetLogLevel(LogLevel level) -> void { CurrentLogLevel = level; }

enum class ConsoleColor : uint8_t {
  Red,         // errors
  Green,       // info
  Blue,        // library messages
  Yellow,      // warnings
  Cyan,        // debug
  Magenta,     // fatal
  BrightWhite, // always
  White,       // general
  Reset        // reset to default
};

auto inline GetColorCode(ConsoleColor color) -> std::string_view {
  switch (color) {
  case ConsoleColor::Red:
    return "\033[31m";
  case ConsoleColor::Green:
    return "\033[32m";
  case ConsoleColor::Blue:
    return "\033[34m";
  case ConsoleColor::Yellow:
    return "\033[33m";
  case ConsoleColor::Cyan:
    return "\033[36m";
  case ConsoleColor::Magenta:
    return "\033[35m";
  case ConsoleColor::BrightWhite:
    return "\033[97m";
  case ConsoleColor::White:
    return "\033[37m";
  default:
    return "\033[0m";
  }
}

auto inline ColorText(const std::string &text, ConsoleColor color) -> std::string {
  return std::format("{}{}{}", GetColorCode(color), text, GetColorCode(ConsoleColor::Reset));
}

auto inline ColorText(const char *text, ConsoleColor color) -> std::string {
  return std::format("{}{}{}", GetColorCode(color), text, GetColorCode(ConsoleColor::Reset));
}

// #define LOG_TO_FILE

#ifdef LOG_TO_FILE
#include <fstream>
// NOLINTBEGIN
static std::ofstream logFile("Snap.log", std::ios::out | std::ios::app);
// NOLINTEND
#define OUT logFile
#define END                                                                                                                                                    \
  '\n';                                                                                                                                                        \
  logFile.flush();
#else
#define OUT std::cout
#define END '\n';
#endif

static std::mutex loggingMutex; // NOLINT
#define OBTAIN_LOG_LOCK std::lock_guard<std::mutex> lock(loggingMutex);

inline void PrintAlways(const std::string &message) {
  OBTAIN_LOG_LOCK
  OUT << ColorText("[ALWAYS] ", ConsoleColor::BrightWhite) << message << END
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintAlways(std::string_view format, Args &&...args) {
  OBTAIN_LOG_LOCK
  OUT << ColorText("[ALWAYS] ", ConsoleColor::BrightWhite) << std::vformat(format, std::make_format_args(args...)) << END
}

inline void PrintDebug(const std::string &message) {
  if (LogLevel::Debug >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[DEBUG] ", ConsoleColor::Cyan) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintDebug(std::string_view format, Args &&...args) {
  if (LogLevel::Debug >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[DEBUG] ", ConsoleColor::Cyan) << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintInfo(const std::string &message) {
  if (LogLevel::Info >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[INFO] ", ConsoleColor::Green) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintInfo(std::string_view format, Args &&...args) {
  if (LogLevel::Info >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[INFO] ", ConsoleColor::Green) << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintLibrary([[maybe_unused]] const std::string &message) {
#ifndef NDEBUG
  if (LogLevel::Debug >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[LIBRARY] ", ConsoleColor::Blue) << message << END
  }
#endif
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintLibrary([[maybe_unused]] std::string_view format, [[maybe_unused]] Args &&...args) {
#ifndef NDEBUG
  if (LogLevel::Debug >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[LIBRARY] ", ConsoleColor::Blue) << std::vformat(format, std::make_format_args(args...)) << END
  }
#endif
}

inline void PrintWarning(const std::string &message) {
  if (LogLevel::Warning >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[WARNING] ", ConsoleColor::Yellow) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintWarning(std::string_view format, Args &&...args) {
  if (LogLevel::Warning >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[WARNING] ", ConsoleColor::Yellow) << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintError(const std::string &message) {
  if (LogLevel::Error >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[ERROR] ", ConsoleColor::Red) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintError(std::string_view format, Args &&...args) {
  if (LogLevel::Error >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[ERROR] ", ConsoleColor::Red) << std::vformat(format, std::make_format_args(args...)) << END
  }
}

inline void PrintFatal(const std::string &message) {
  if (LogLevel::Fatal >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[FATAL] ", ConsoleColor::Magenta) << message << END
  }
}

template <typename... Args> // NOLINTNEXTLINE args forwarding
inline void PrintFatal(std::string_view format, Args &&...args) {
  if (LogLevel::Fatal >= CurrentLogLevel) {
    OBTAIN_LOG_LOCK
    OUT << ColorText("[FATAL] ", ConsoleColor::Magenta) << std::vformat(format, std::make_format_args(args...)) << END
  }
}