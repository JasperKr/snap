#include "taperedCapsuleShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaTaperedCapsuleShape::wrap_GetTopRadius(lua_State *state) -> int { return 0; }
auto LuaTaperedCapsuleShape::wrap_GetBottomRadius(lua_State *state) -> int { return 0; }
auto LuaTaperedCapsuleShape::wrap_GetHalfHeight(lua_State *state) -> int { return 0; }
auto LuaTaperedCapsuleShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaTaperedCapsuleShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics