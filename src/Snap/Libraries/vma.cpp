#include "../Modules/console.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#define VMA_DEBUG_LOG(format, ...) PrintLibrary(format, ##__VA_ARGS__)
#define VMA_IMPLEMENTATION
// #define VMA_DEBUG_MARGIN 16
// #define VMA_DEBUG_DETECT_CORRUPTION 1

// Define macro VMA_DEBUG_LOG_FORMAT or more specialized VMA_LEAK_LOG_FORMAT
// to receive the list of the unfreed allocations.

// #ifndef NDEBUG
// formatted with %s etc, we cannot PrintLibrary directly since it uses std::format
// We must first format the string ourselves
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                                                                                                      \
  do {                                                                                                                                                         \
    char buf[256];                                                                                                                                             \
    std::snprintf(buf, sizeof(buf), format, __VA_ARGS__);                                                                                                      \
    PrintLibrary(std::string("VMA: ") + buf);                                                                                                                  \
  } while (0)
// #endif

#include "vma.hpp"