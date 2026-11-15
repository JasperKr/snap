#include "planeShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaPlaneShape::wrap_GetPlane(lua_State *state) -> int { return 0; }
auto LuaPlaneShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaPlaneShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics