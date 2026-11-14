#include "boxShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaBoxShape::wrap_GetHalfExtent(lua_State *state) -> int { return 0; }
auto LuaBoxShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaBoxShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics