#pragma once

#include <condition_variable>
#include <optional>
#include <queue>

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/lua_data.hpp"
#include <mutex>
#include <string>
#include <vector>

#include "lua.hpp"

namespace Threading {

const static Type channelType = Type("Channel");

// Channel storage for shutdown
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<struct Channel>> Channels;

auto AddReferences(const LuaWrap::Data::LuaType &data) -> void;
auto RemoveReferences(const LuaWrap::Data::LuaType &data) -> void;
auto UnloadChannelModule() -> void;

struct Channel : Object {
public:
  Channel() = default;
  Channel(const Channel &) = delete;
  Channel(Channel &&) = delete;
  auto operator=(const Channel &) -> Channel & = delete;
  auto operator=(Channel &&) -> Channel & = delete;
  // Push a message to the channel
  auto Push(const LuaWrap::Data::LuaType &message) -> void;

  // Pop a message from the channel
  auto Pop() -> std::optional<LuaWrap::Data::LuaType>;

  // Peek at the next message without removing it
  [[nodiscard]]
  auto Peek() const -> std::optional<LuaWrap::Data::LuaType>;

  [[nodiscard]]
  auto GetCount() const -> size_t;

  // Demand a message, blocking until one is available
  auto Demand(double timeout) -> std::optional<LuaWrap::Data::LuaType>;
  auto Demand() -> std::optional<LuaWrap::Data::LuaType>;

  // Clear all messages from the channel
  auto Clear() -> void;

  auto GetInstanceType() const -> Type const * override { return &channelType; }
  static auto GetType() -> Type const * { return &channelType; }

  auto IsDestroyed() const -> bool {
    std::lock_guard<std::mutex> lock(isDestroyedMutex);
    return isDestroyed;
  }

  auto DestroyImmediately() -> void {
    {
      std::lock_guard<std::mutex> lock(mutex);
      // Iterate through messages and release references
      while (!messages.empty()) {
        RemoveReferences(messages.front());

        messages.pop();
      }
    }

    std::lock_guard<std::mutex> lock(isDestroyedMutex);
    isDestroyed = true;

    // Notify all waiting threads
    condition.notify_all();
  }

  ~Channel() override { DestroyImmediately(); };

private:
  std::queue<LuaWrap::Data::LuaType> messages;

  // Mutex for thread safety
  mutable std::mutex mutex;
  mutable std::condition_variable condition;

  mutable std::mutex isDestroyedMutex;
  mutable bool isDestroyed = false;
};

} // namespace Threading
