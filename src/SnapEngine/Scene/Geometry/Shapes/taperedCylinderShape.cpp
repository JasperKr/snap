#include "taperedCylinderShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaTaperedCylinderShape::wrap_GetRadius(lua_State *state) -> int { return 0; }
auto LuaTaperedCylinderShape::wrap_GetHalfHeight(lua_State *state) -> int { return 0; }
auto LuaTaperedCylinderShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaTaperedCylinderShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics