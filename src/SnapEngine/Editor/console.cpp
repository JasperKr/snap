#pragma once

#include "console.hpp"
#include "Modules/error.hpp"
#include <imgui.h>
#include <lua.hpp>
#include <string>
#include <string_view>
#include <vector>
namespace Editor {

auto Console::Evaluate(lua_State *state, const std::string &command) -> Error {
  Lines.emplace_back(command);

  return {};
}
auto Console::Print(std::string_view &line) -> void {}
auto Console::Draw() -> Error {
  if (ImGui::Begin("Console")) {
    //...
  }
  ImGui::End();

  return {};
}

} // namespace Editor