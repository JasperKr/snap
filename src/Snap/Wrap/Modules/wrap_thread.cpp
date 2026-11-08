#include "wrap_thread.hpp"
#include "Modules/error.hpp"
#include "Modules/thread.hpp"
#include "Wrap/wrap.hpp"
#include <lua.hpp>
#include <string>

namespace Wrap::Threading {

const auto threadStatusEnum = LuaWrap::LuaEnum<::Threading::ThreadStatus>("ThreadStatus", {
                                                                                              {"running", ::Threading::ThreadStatus::Running},
                                                                                              {"finished", ::Threading::ThreadStatus::Stopped},
                                                                                              {"error", ::Threading::ThreadStatus::Error},
                                                                                          });

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<Ref<::Threading::Thread>> Threads;

auto wrap_NewThread(lua_State *state) -> int {
  const auto *script = luaL_checkstring(state, 1);

  // NOLINTNEXTLINE
  auto thread = ::Threading::Thread::Create(script);

  // Optional name parameter
  if (lua_gettop(state) >= 2) {
    const auto *name = luaL_checkstring(state, 2);
    thread->SetDebugName(name);
  }

  LuaWrap::PushObject(state, ::Threading::Thread::GetType(), thread.get());

  Threads.emplace_back(thread);

  return 1;
}

auto wrap_StartThread(lua_State *state) -> int {
  auto thread = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Thread>(state, 1));

  int numArgs = lua_gettop(state) - 1;
  auto result = LuaWrap::SerializeVarargs(state, 2);

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to build launch arguments: %s", result.error().message.c_str());
  }

  thread->Start(result.value(), numArgs);

  return 0;
}
auto wrap_WaitThread(lua_State *state) -> int {
  auto thread = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Thread>(state, 1));

  thread->Wait();

  return 0;
}
auto wrap_GetThreadStatus(lua_State *state) -> int {
  auto thread = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Thread>(state, 1));

  auto status = thread->GetStatus();

  switch (status) {
  case ::Threading::ThreadStatus::Running:
    lua_pushstring(state, "running");
    break;
  case ::Threading::ThreadStatus::Stopped:
    lua_pushstring(state, "stopped");
    break;
  case ::Threading::ThreadStatus::Error:
    lua_pushstring(state, "error");
    break;
  default:
    lua_pushstring(state, "unknown");
    break;
  }

  return 1;
}
auto wrap_GetThreadErrorMessage(lua_State *state) -> int {
  auto thread = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Thread>(state, 1));
  if (thread->GetStatus() != ::Threading::ThreadStatus::Error) {
    lua_pushnil(state);
    return 1;
  }

  auto errorMessage = thread->GetErrorMessage();
  lua_pushstring(state, errorMessage.c_str());

  return 1;
}

} // namespace Wrap::Threading