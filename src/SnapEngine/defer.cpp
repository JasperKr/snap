#include "defer.hpp"

DeferredManager DeferredManager::Instance{};
void DeferredManager::Register(const Deferred &action) {
  std::lock_guard<std::mutex> lock(mutex);
  actions.emplace_back(action);
}
void DeferredManager::Update() {
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
void DeferredManager::Shutdown() {
  for (auto &action : actions) {
    if (action.callOnShutdown) {
      action.action();
    }
  }

  actions.clear();
}
