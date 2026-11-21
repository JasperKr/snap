#pragma once

#include "Modules/Helpers/utils.hpp"
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

struct Deferred {
  // If the application is shut down before the frame count passed, call this action on shutdown
  // If left to false it will just shutdown without calling it.
  bool callOnShutdown = false;
  uint64_t frames{};
  std::function<void(void)> action;
};

struct DeferredManager {
  std::vector<Deferred> actions;
  std::mutex mutex;

  void Register(const Deferred &action);

  void Update();

  void Shutdown();

  static DeferredManager Instance;
};