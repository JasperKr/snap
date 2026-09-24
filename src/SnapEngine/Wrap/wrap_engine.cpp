#include "Wrap/wrap_editor.hpp"
#include "Wrap/wrap_imgui.hpp"
#include "Wrap/wrap_renderer.hpp"
// #include "Wrap/wrap_scene.hpp"
#include "Wrap/wrap_scene.hpp"
#include <lua.hpp>
#include <vector>

namespace Engine::LuaWrap {

static const std::vector<luaL_Reg> SnapModules = {
    {"gui", Wrap::Imgui::luaopen_gui},
    {"scene", Wrap::Engine::luaopen_scene},
    {"renderer", Engine::Renderer::luaopen_renderer},
    {"editor", Engine::Wrap_Editor::luaopen_editor},
};

auto RegisterModules(lua_State *state) -> void {
  lua_getglobal(state, "snap"); // [snap or nil]
  if (!lua_istable(state, -1)) {
    lua_newtable(state);          // [nil, table]
    lua_setglobal(state, "snap"); // set global 'snap'
  }
  // At this point, 'snap' table is on top of stack
  lua_pop(state, 1); // empty stack

  for (const auto &module : SnapModules) {
    module.func(state);
  }
}

} // namespace Engine::LuaWrap