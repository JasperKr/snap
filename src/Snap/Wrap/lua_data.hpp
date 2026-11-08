#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <string>
#include <variant>
#include <vector>

#include "lua.hpp"

namespace LuaWrap::Data {

using LuaType = std::variant<std::monostate, bool, double, std::string, Proxy, std::vector<struct LuaData>>;

struct LuaData {
  LuaType key;
  LuaType value;
};

auto FromStack(lua_State *state, int index) -> Result<LuaType>;
auto ToStack(lua_State *state, const LuaType &data) -> Error;

} // namespace LuaWrap::Data