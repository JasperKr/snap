#include "wrap_renderer.hpp"
#include "Wrap/wrap.hpp"
#include "renderer.hpp"

namespace Engine::Renderer {
auto wrap_NewMaterial(lua_State *state) -> int { return 0; }

auto wrap_Initialize(lua_State *state) -> int {
  LUA_CK_ERR(
      RendererInstance.Initialize(*Graphics::GetCurrentGraphicsContext()));

  return 0;
}

auto wrap_ReloadShaders(lua_State *state) -> int {
  RendererInstance.GetShaderManager().ReloadShaders();
  return 0;
}

} // namespace Engine::Renderer