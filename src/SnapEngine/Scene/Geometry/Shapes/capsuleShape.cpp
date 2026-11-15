#include "capsuleShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaCapsuleShape::wrap_GetRadius(lua_State *state) -> int { return 0; }
auto LuaCapsuleShape::wrap_GetHalfHeight(lua_State *state) -> int { return 0; }
auto LuaCapsuleShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaCapsuleShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics