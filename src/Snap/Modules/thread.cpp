#include "thread.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/renderThread.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "Wrap/lua_data.hpp"
#include "Wrap/wrap.hpp"
#include "event.hpp"
#include <csignal>
#include <cstdint>
#include <mutex>
#include <public/common/TracySystem.hpp>
#include <thread>
#include <vector>

#include "loop.hpp"
#include <lua.hpp>

#include "channel.hpp"

namespace Threading {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> IsShuttingDown;

inline auto GetThreadStatusChannel() -> Ref<Channel> & {
  static auto threadStatusChannel = Ref<Channel>::Make();
  return threadStatusChannel;
}

auto Thread::Create(const std::string &script) -> Ref<Thread> {
  auto thread = Ref<Thread>::Make();
  thread->script = script;
  return thread;
}

auto Thread::Close(ThreadStatus status, const std::string &message) -> Error {
  CHECK_ERR(Graphics::Threading::Deinitialize(*Graphics::GetCurrentGraphicsContext()));

  if (state != nullptr) {
    lua_close(state);
  }

  {
    std::lock_guard<std::mutex> lock(statusMutex);
    this->status = status;
    this->errorMessage = message;
  }

  if (status == ThreadStatus::Error) {
    // PrintWarning("Thread encountered an error: {}", message);
    Event::Push(Event::Event{
        .Name = "threaderror",
        .Values = {message},
    });
  }

  statusCV.notify_all();

  this->release(); // Releases self-ownership

  return {};
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local ThreadID CurrentThreadID = 0;

auto Thread::Run(Thread *thread, const std::vector<LuaWrap::Data::LuaType> &launchArguments, int count, ThreadID identifier) -> void { // NOLINT
  CurrentThreadID = identifier;
  tracy::SetThreadName(thread->debugname.c_str());

  lua_State *state = luaL_newstate();
  luaL_openlibs(state);
  thread->state = state;
  thread->debugname = // NOLINTNEXTLINE
      "Thread_" + std::to_string(reinterpret_cast<uintptr_t>(thread));

  lua_getglobal(state, "package");
  lua_getfield(state, -1, "path");
  std::string currentPath = lua_tostring(state, -1);
  lua_pop(state, 1); // remove original path
  std::string newPath = Path::Join(Filesystem::GetSourceDirectory(), std::string("?.lua;")) + currentPath;
  lua_pushstring(state, newPath.c_str());
  lua_setfield(state, -2, "path");
  lua_pop(state, 1); // remove package table

  RegisterAllLuaModules(state);

  Graphics::ContextDebugname = thread->debugname;
  PrintInfo("Initializing graphics with debug name: {}", Graphics::ContextDebugname);

  auto err = Graphics::Threading::Initialize(*Graphics::GetCurrentGraphicsContext());

  if (Error::IsError(err)) {
    err = thread->Close(ThreadStatus::Error, err.message);
    return;
  }

  bool isPath = (thread->script.size() > 4 &&
                 (thread->script.substr(thread->script.size() - 4) == ".lua" || thread->script.substr(thread->script.size() - 3) == ".luac"));

  int result = 0;

  if (isPath) {
    result = luaL_loadfile(state, thread->script.c_str());
  } else {
    result = luaL_loadstring(state, thread->script.c_str());
  }

  if (result == LUA_OK) {
    auto error = LuaWrap::PushVarargs(state, launchArguments, count);

    if (Error::IsError(error)) {
      err = thread->Close(ThreadStatus::Error, error.message);
      return;
    }

    result = lua_pcall(state, count, 0, 0);
  }

  // Stop rendering if still active (crashed or user error, for example)
  if (Graphics::Threading::CurrentRenderThreadInfo.get() != nullptr) {
    PrintWarning("thread was still rendering when exiting.");
  }

  if (result != LUA_OK) {
    const auto *luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1);

    err = thread->Close(ThreadStatus::Error, luaErrorMessage != nullptr ? std::string(luaErrorMessage) : "Unknown Lua error occurred.");
    return;
  }

  err = thread->Close(ThreadStatus::Stopped, "");
}

auto Thread::Start(const std::vector<LuaWrap::Data::LuaType> &launchArguments, int count) -> void {

  this->retain(); // Owns itself

  auto handle = std::thread(Threading::Thread::Run, this, launchArguments, count, ThreadIDCounter.fetch_add(1) + 1);

  handle.detach();
}

auto Thread::GetStatus() const -> ThreadStatus {
  std::lock_guard<std::mutex> lock(statusMutex);
  return status;
}

auto Thread::Wait() -> void {
  // Wait until status is not Running
  std::unique_lock<std::mutex> lock(statusMutex);
  statusCV.wait(lock, [this]() -> bool { return this->status != ThreadStatus::Running; });
}

auto Thread::GetErrorMessage() const -> std::string {
  std::lock_guard<std::mutex> lock(statusMutex);
  return errorMessage;
}

auto UnloadModule() -> void { UnloadChannelModule(); }

inline auto OnSIGINT(int signal) -> void {
  IsShuttingDown.store(true);
  UnloadChannelModule();
}

auto LoadModule() -> void { std::signal(SIGINT, OnSIGINT); }

} // namespace Threading