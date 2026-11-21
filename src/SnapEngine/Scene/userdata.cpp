#include "userdata.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <imgui.h>
#include <lua.hpp>

namespace Engine {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
thread_local std::vector<int32_t> FreedUserdataIndices{};

auto Userdata::SetUserdata(lua_State *state) -> int {
  // Stack: [entity, userdata]

  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 2) {
    return luaL_error(state, "Expected 1 argument for userdata");
  }

  auto *userdata = entity->try_get_mut<Userdata>();
  if (userdata == nullptr) {
    entity->set<Userdata>({});
    userdata = entity->try_get_mut<Userdata>();
  }

  if (userdata->userdataIndex == 0) {
    userdata->userdataIndex = GetFreeUserdataIndex();
  }

  ::LuaWrap::SetStackToRegistry(state, "Userdata");
  // Stack: [entity, userdata, "Userdata"]
  lua_pushvalue(state, 2);
  // Stack: [entity, userdata, "Userdata", userdata]

  lua_rawseti(state, -2, static_cast<int>(userdata->userdataIndex));
  // raw set integer -> Userdata[userdata->userdataIndex] = userdata
  // Stack: [entity, userdata, "Userdata"]

  // Return the userdata the user provided for convenience
  lua_pop(state, 1); // Pop "Userdata" table
  // Stack: [entity, userdata]

  return 1;
}

auto Userdata::GetUserdata(lua_State *state) -> int {
  // Stack: [entity]

  auto *entity = ::LuaWrap::EntityFromLua(state, 1);

  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *userdata = entity->try_get<Userdata>();
  if (userdata == nullptr || userdata->userdataIndex == 0) {
    lua_pushnil(state);
    return 1;
  }

  ::LuaWrap::SetStackToRegistry(state, "Userdata");
  // Stack: [entity, "Userdata"]
  lua_rawgeti(state, -1, static_cast<int>(userdata->userdataIndex));
  // Stack: [entity, "Userdata", userdata]
  return 1;
}

inline auto LuaToString(lua_State *state, int index) -> std::string {
  if (lua_isstring(state, index) == 0) {
    return "<non-string data>";
  }
  return lua_tostring(state, index);
}

inline auto DrawTable(lua_State *state, int index) -> void {
  lua_pushnil(state);
  while (lua_next(state, index) != 0) {
    // Stack: [table, key, value]
    ImGui::Text("%s: ", LuaToString(state, -2).c_str());

    if (lua_istable(state, -1)) {
      ImGui::Indent();
      DrawTable(state, lua_gettop(state));
      ImGui::Unindent();
    } else {
      ImGui::SameLine();
      ImGui::Text("%s", LuaToString(state, -1).c_str());
    }

    lua_pop(state, 1); // Pop value, keep key for next iteration
  }
  lua_pop(state, 1); // Pop the key
}

auto Userdata::DrawGUI(lua_State *state) const -> void {
  ImGui::Text("Userdata Index: %d", userdataIndex);
  if (state == nullptr) {
    return;
  }

  ::LuaWrap::SetStackToRegistry(state, "Userdata");
  lua_rawgeti(state, -1, static_cast<int>(userdataIndex));

  if (lua_isnil(state, -1)) {
    ImGui::Text("No userdata associated with this index");
    lua_pop(state, 1);
    return;
  }

  if (lua_istable(state, -1)) {
    DrawTable(state, lua_gettop(state));
  } else {
    ImGui::Text("Userdata value: %s", LuaToString(state, -1).c_str());
  }
}

const ::LuaWrap::LuaComponent UserdataComponent{{
    {"setUserdata", Userdata::SetUserdata},
    {"getUserdata", Userdata::GetUserdata},
}};

auto Userdata::GetFreeUserdataIndex() -> int32_t {
  static int32_t currentIndex = 1;
  if (!FreedUserdataIndices.empty()) {
    int32_t index = FreedUserdataIndices.back();
    FreedUserdataIndices.pop_back();
    return index;
  }
  return currentIndex++;
}
auto Userdata::OnRemove() -> void {
  if (userdataIndex != 0) {
    FreeUserdataIndex(userdataIndex);
    userdataIndex = 0;
  }
}
} // namespace Engine