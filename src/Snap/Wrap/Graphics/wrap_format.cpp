#include "wrap_format.hpp"
#include "Graphics/bufferformat.hpp"
#include "Graphics/format.hpp"
#include "Modules/error.hpp"
#include <string>
#include <vector>

#include <lua.hpp>

namespace Wrap::Graphics {
auto ComponentFromLua(lua_State *state, int index) -> Result<::Graphics::BufferComponent> {

  auto component = ::Graphics::BufferComponent();

  lua_getfield(state, -1, "name");
  if (!lua_isnoneornil(state, -1)) {
    component.name = luaL_checkstring(state, -1);
  }
  lua_pop(state, 1);

  lua_getfield(state, -1, "arraysize");
  if (!lua_isnoneornil(state, -1)) {
    component.arraySize = luaL_checkinteger(state, -1);
  }
  lua_pop(state, 1);

  lua_getfield(state, -1, "format");

  if (lua_type(state, -1) == LUA_TSTRING) {
    const auto &str = luaL_checkstring(state, -1);
    component.format = ::Graphics::Format::FromString(str);
    component.arraySize *= ::Graphics::Format::StringToArraySize(str);
    lua_pop(state, 1);
    return component;
  }

  if (lua_type(state, -1) == LUA_TTABLE) {
    auto result = FormatFromLua(state, -1);
    lua_pop(state, 1);
    if (Error::IsError(result)) {
      return result.error();
    }

    component.format = result.value();
    return component;
  }

  auto luatypename = std::string(luaL_typename(state, -1));

  lua_pop(state, 1);
  return Error::Unexpectedf("Buffer format: Expected string or table, got {}", luatypename);
}

auto FormatFromLua(lua_State *state, int index, ::Graphics::Standard standard) -> Result<::Graphics::BufferFormat> {
  std::vector<::Graphics::BufferComponent> components;

  for (int i = 0; i < lua_objlen(state, index); i++) {
    lua_rawgeti(state, index, i + 1); // stack + index

    auto component = CHECK_RES(ComponentFromLua(state, -1));
    components.emplace_back(component);
    lua_pop(state, 1);
  }

  return ::Graphics::BufferFormat(components, standard);
}

auto SimpleFormatFromLua(lua_State *state, int index, ::Graphics::Standard standard) -> Result<::Graphics::BufferFormat> {
  std::vector<::Graphics::BufferComponent> components;

  if (lua_type(state, index) == LUA_TSTRING) {
    const auto &str = luaL_checkstring(state, index);
    auto format = ::Graphics::Format::FromString(str);
    components.emplace_back(::Graphics::BufferComponent{.name = "Default", .format = format});
  } else {
    auto luatypename = std::string(luaL_typename(state, index));
    return Error::Unexpectedf("Buffer format: Expected string, got {}", luatypename);
  }

  return ::Graphics::BufferFormat(components, standard);
}
} // namespace Wrap::Graphics