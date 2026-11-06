#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/lua_data.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "lua.hpp"

// Thread system for lua scripts
// Each thread has its own lua_State and runs independently
// Threads can communicate via channels
// Channels are thread-safe queues that can hold messages
// Messages can be of any type that is serializable:
// numbers, strings, booleans, tables, but not functions or userdata except for any engine-defined types

namespace Threading {

using ThreadID = uint32_t;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local extern ThreadID CurrentThreadID;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<ThreadID> ThreadIDCounter = 0;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::atomic<bool> IsShuttingDown;

auto UnloadModule() -> void;

auto LoadModule() -> void;

enum class ThreadStatus : uint8_t { Running, Stopped, Error };

const static Type threadType = Type("Thread");

struct Thread : Object {
public:
  Thread() = default;
  Thread(const Thread &) = delete;
  Thread(Thread &&) = delete;
  auto operator=(const Thread &) -> Thread & = delete;
  auto operator=(Thread &&) -> Thread & = delete;
  ~Thread() override = default;

  static auto Create(const std::string &script) -> Ref<Thread>;

  auto Start(const std::vector<LuaWrap::Data::LuaType> &launchArguments, int count) -> void;
  [[nodiscard]] auto GetStatus() const -> ThreadStatus;
  [[nodiscard]] auto GetErrorMessage() const -> std::string;
  auto Wait() -> void;

  auto GetInstanceType() const -> Type const * override { return &threadType; }
  static auto GetType() -> Type const * { return &threadType; }

  auto SetDebugName(const std::string &name) -> void { debugname = name; }

private:
  static auto Run(Thread *thread, const std::vector<LuaWrap::Data::LuaType> &launchArguments, int count, ThreadID identifier) -> void;
  auto Close(ThreadStatus status, const std::string &message) -> Error;

  std::string script;
  lua_State *luaState = nullptr;

  mutable std::mutex statusMutex;
  mutable std::condition_variable statusCV;
  ThreadStatus status{ThreadStatus::Running};
  std::string errorMessage;
  lua_State *state = nullptr;

  // Auto-generated name for the thread
  std::string debugname;
};

} // namespace Threading