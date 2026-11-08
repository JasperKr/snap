#pragma once

#include "Modules/bindings.hpp"
#include "Modules/object.hpp"
#include "Modules/reflectBindings.hpp"
#include "Modules/thread.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/Modules/wrap_channel.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include <vector>
namespace Wrap::Threading {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::vector<Ref<::Threading::Thread>> Threads;

auto wrap_NewThread(lua_State *state) -> int;
auto wrap_StartThread(lua_State *state) -> int;
auto wrap_WaitThread(lua_State *state) -> int;
auto wrap_GetThreadStatus(lua_State *state) -> int;
auto wrap_GetThreadErrorMessage(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ThreadLib = {
    {"start", wrap_StartThread},
    {"wait", wrap_WaitThread},
    {"getStatus", wrap_GetThreadStatus},
    {"getError", wrap_GetThreadErrorMessage},

};

extern "C" inline auto luaopen_thread(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Threading::Thread::GetType(),
                           ThreadLib); // NOLINT

  return 1;
}

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ThreadingLib = {
    {"newThread", wrap_NewThread},
    {"newChannel", Wrap::Threading::wrap_NewChannel},

};

const static std::vector<lua_CFunction> childrenInitFunctions = {
    luaopen_thread,
    Wrap::Threading::luaopen_channel,
};

const extern LuaWrap::LuaEnum<::Threading::ThreadStatus> threadStatusEnum;

extern "C" inline auto luaopen_threading(lua_State *state) -> int {
  auto module = LuaWrap::LuaModule{
      .Name = "thread",
      .Functions = ThreadingLib,                      // NOLINT
      .ChildrenInitFunctions = childrenInitFunctions, // NOLINT

  };

  RegisterLuaModule(state, module);

  auto documenter = Bindings::LuaDocumentingStruct("Thread");

  auto threadStatusType = Bindings::TypeInfo{
      .name = "ThreadStatus",
      .type = nullptr,
      .luaType = Bindings::BindingLuaType::String,
      .isEnum = true,
      .isVector = false,
      .enumHelper = &threadStatusEnum,
  };

  auto thisType = Bindings::TypeInfo{
      .name = "Thread",
      .type = ::Threading::Thread::GetType(),
      .luaType = Bindings::BindingLuaType::Userdata,
      .isEnum = false,
      .isVector = false,
      .enumHelper = nullptr,
  };

  documenter.DocumentCustomMethod("newThread", "Create a new thread from the given filepath",
                                  {Bindings::LuaDocumentingStruct::CreateType("Path", Bindings::BindingLuaType::String),
                                   Bindings::LuaDocumentingStruct::CreateType("Debugname", Bindings::BindingLuaType::String)},
                                  thisType);

  documenter.DocumentCustomMethod("newThread", "Create a new thread from the given lua code string",
                                  {Bindings::LuaDocumentingStruct::CreateType("Code", Bindings::BindingLuaType::String),
                                   Bindings::LuaDocumentingStruct::CreateType("Debugname", Bindings::BindingLuaType::String)},
                                  thisType);

  documenter.DocumentCustomMethod("start", "Start the thread", {Bindings::LuaDocumentingStruct::CreateType("...", Bindings::BindingLuaType::ThreadSafe)}, {});

  documenter.DocumentCustomMethod("wait", "Wait for the thread to finish");
  documenter.DocumentCustomMethod("getStatus", "Get the current status of the thread", {}, threadStatusType);

  documenter.DocumentCustomMethod("getError", "Get the error message if the thread has errored", {},
                                  Bindings::LuaDocumentingStruct::CreateType("ErrorMessage", Bindings::BindingLuaType::String));

  documenter.Register();

  return 1;
}

} // namespace Wrap::Threading