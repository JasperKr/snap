#include "heightFieldShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaHeightFieldShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaHeightFieldShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics