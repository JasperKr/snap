#include "wrap_channel.hpp"
#include "Modules/channel.hpp"
#include "Wrap/lua_data.hpp"
#include "Wrap/wrap.hpp"
#include <lua.hpp>

namespace Wrap::Threading {

auto wrap_NewChannel(lua_State *state) -> int {
  // NOLINTNEXTLINE
  auto channel = Ref<::Threading::Channel>::Make();

  ::Threading::Channels.emplace_back(channel);

  LuaWrap::PushObject(state, ::Threading::Channel::GetType(), channel.get());

  return 1;
}

auto wrap_Push(lua_State *state) -> int {
  auto channel = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Channel>(state, 1));

  if (lua_gettop(state) < 2) {
    return luaL_error(state, "Channel:push requires a message argument.");
  }

  auto encodedMessage = LuaWrap::Data::FromStack(state, 2);

  if (Error::IsError(encodedMessage)) {
    return luaL_error(state, "Failed to serialize message: %s", encodedMessage.error().message.c_str());
  }

  ::Threading::AddReferences(encodedMessage.value());
  channel->Push(encodedMessage.value());

  return 0;
}

auto wrap_Pop(lua_State *state) -> int {
  auto channel = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Channel>(state, 1));

  auto message = channel->Pop();

  if (!message.has_value()) {
    lua_pushnil(state);
    return 1;
  }

  auto error = LuaWrap::Data::ToStack(state, message.value());
  ::Threading::RemoveReferences(message.value());

  LUA_CK_ERR(error);

  return 1;
}

auto wrap_Peek(lua_State *state) -> int {
  auto channel = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Channel>(state, 1));
  if (channel == nullptr) {
    return luaL_error(state, "Invalid Channel object.");
  }

  auto message = channel->Peek();

  if (!message.has_value()) {
    lua_pushnil(state);
    return 1;
  }

  auto error = LuaWrap::Data::ToStack(state, message.value());

  if (Error::IsError(error)) {
    return luaL_error(state, "Failed to deserialize message: %s", error.message.c_str());
  }

  return 1;
}
auto wrap_GetCount(lua_State *state) -> int {
  auto channel = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Channel>(state, 1));
  if (channel == nullptr) {
    return luaL_error(state, "Invalid Channel object.");
  }

  auto count = channel->GetCount();

  lua_pushinteger(state, static_cast<lua_Integer>(count));

  return 1;
}
auto wrap_Demand(lua_State *state) -> int {
  auto channel = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Channel>(state, 1));
  if (channel == nullptr) {
    return luaL_error(state, "Invalid Channel object.");
  }

  std::optional<LuaWrap::Data::LuaType> messageResult;
  if (lua_gettop(state) >= 2) {
    if (lua_isnumber(state, 2) == 0) {
      return luaL_error(state, "Timeout argument must be a number.");
    }

    auto timeout = lua_tonumber(state, 2);

    messageResult = channel->Demand(timeout);
  } else {
    messageResult = channel->Demand();
  }

  if (!messageResult.has_value()) {
    lua_pushnil(state);
    return 1;
  }

  auto &message = messageResult.value();
  auto error = LuaWrap::Data::ToStack(state, message);
  ::Threading::RemoveReferences(message);

  if (Error::IsError(error)) {
    return luaL_error(state, "Failed to deserialize message: %s", error.message.c_str());
  }

  return 1;
}

auto wrap_Clear(lua_State *state) -> int {
  auto channel = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Threading::Channel>(state, 1));
  if (channel == nullptr) {
    return luaL_error(state, "Invalid Channel object.");
  }

  channel->Clear();

  return 0;
}

} // namespace Wrap::Threading
