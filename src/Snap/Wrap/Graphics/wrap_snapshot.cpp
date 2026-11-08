#include "wrap_snapshot.hpp"
#include "Graphics/snapshot.hpp"
#include "Wrap/wrap.hpp"
#include <lua.hpp>

namespace Wrap::Graphics::Snapshot {

auto wrap_Draw(lua_State *state) -> int {
  auto snapshot = LUA_CK_NULL(LuaWrap::ObjectFromLua<::Graphics::Snapshot::ThreadSnapshot>(state, 1));

  RenderSnapshot(*snapshot);

  return 0;
}

} // namespace Wrap::Graphics::Snapshot
