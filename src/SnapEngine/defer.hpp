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

  void Register(const Deferred &action) {
    std::lock_guard<std::mutex> lock(mutex);
    actions.emplace_back(action);
  }

  void Update() {
    std::lock_guard<std::mutex> lock(mutex);

    for (auto &action : actions) {
      if (action.frames == 0) {
        action.action();
        action.frames = UINT64_MAX;
      } else {
        action.frames--;
      }
    }

    Utils::UnorderedErase(actions, [](const Deferred &action) -> bool { return action.frames == UINT64_MAX; });
  }

  void Shutdown() {
    for (auto &action : actions) {
      if (action.callOnShutdown) {
        action.action();
      }
    }

    actions.clear();
  }

  static DeferredManager Instance;
};