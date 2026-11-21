#pragma once

#include "Modules/error.hpp"
#include <lua.hpp>
#include <string>
#include <string_view>
#include <vector>
namespace Editor {

struct Console {
  std::vector<std::string> Lines;

  auto Evaluate(lua_State *state, const std::string &command) -> Error;
  auto Print(std::string_view &line) -> void;
  auto Draw() -> Error;
};

} // namespace Editor