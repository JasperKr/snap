#pragma once

#include "Modules/error.hpp"
#include "Wrap/wrap.hpp"

#include "lua.hpp"

namespace Engine {
class UiElement {
public:
  UiElement(const UiElement &) = default;
  UiElement(UiElement &&) = delete;
  UiElement() = default;
  auto operator=(const UiElement &) -> UiElement & = default;
  auto operator=(UiElement &&) -> UiElement & = delete;
  virtual ~UiElement() = default;

  virtual auto DrawUiElement() -> Error = 0;

  template <typename T> static constexpr auto GetDrawUiElementLuaBinding() -> lua_CFunction {
    return [](lua_State *state) -> int {
      auto *uiElement = LuaWrap::ObjectFromLua<T>(state, 1);
      if (uiElement == nullptr) {
        return luaL_error(state, "Invalid UiElement object");
      }

      auto error = uiElement->DrawUiElement();
      if (Error::IsError(error)) {
        return luaL_error(state, "Error drawing UI element: %s", error.message.c_str());
      }

      return 0;
    };
  }
};

} // namespace Engine