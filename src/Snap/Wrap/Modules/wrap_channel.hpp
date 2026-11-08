#pragma once

#include "Modules/bindings.hpp"
#include "Modules/channel.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
namespace Wrap::Threading {

auto wrap_NewChannel(lua_State *state) -> int;
auto wrap_Push(lua_State *state) -> int;
auto wrap_Pop(lua_State *state) -> int;
auto wrap_Peek(lua_State *state) -> int;
auto wrap_GetCount(lua_State *state) -> int;
auto wrap_Demand(lua_State *state) -> int;
auto wrap_Clear(lua_State *state) -> int;

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> ChannelLib = {
    {"push", wrap_Push}, {"pop", wrap_Pop}, {"peek", wrap_Peek}, {"getCount", wrap_GetCount}, {"demand", wrap_Demand}, {"clear", wrap_Clear},

};

extern "C" inline auto luaopen_channel(lua_State *state) -> int {
  LuaWrap::RegisterLuaType(state, ::Threading::Channel::GetType(),
                           ChannelLib); // NOLINT

  const auto &channelSendableType = Bindings::LuaDocumentingStruct::CreateType("Value", Bindings::BindingLuaType::ThreadSafe);

  auto documenter = Bindings::LuaDocumentingStruct("Channel");
  constexpr auto &createType = Bindings::LuaDocumentingStruct::CreateType;
  documenter.DocumentCustomMethod("push", "Push a value into the channel", {channelSendableType});

  documenter.DocumentCustomMethod("pop", "Pop a value from the channel, or return nil if the channel is empty", {}, channelSendableType);

  documenter.DocumentCustomMethod("peek", "Peek at the next value in the channel without popping it", {}, channelSendableType);

  documenter.DocumentCustomMethod("getCount", "Get the number of items currently in the channel", {}, createType("Count", Bindings::BindingLuaType::Integer));

  documenter.DocumentCustomMethod("demand",
                                  "Pop a value from the channel, or yield until a value is available if "
                                  "the channel is empty",
                                  {}, channelSendableType);
  documenter.DocumentCustomMethod("demand",
                                  "Pop a value from the channel, or yield until a value is available if "
                                  "the channel is empty. If the channel is closed while waiting, returns "
                                  "nil.",
                                  {createType("Timeout", Bindings::BindingLuaType::Number)}, channelSendableType);
  documenter.DocumentCustomMethod("clear", "Clear all items from the channel, leaving it empty", {});

  documenter.Register();

  return 1;
}

} // namespace Wrap::Threading