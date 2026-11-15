#include "convexHullShape.hpp"

namespace Engine::Physics {

static auto Create(lua_State *state) -> int { return 0; }

auto LuaConvexHullShape::wrap_GetMass(lua_State *state) -> int { return 0; }
auto LuaConvexHullShape::wrap_GetInertiaTensor(lua_State *state) -> int { return 0; }
auto LuaConvexHullShape::wrap_GetPlanes(lua_State *state) -> int { return 0; }
auto LuaConvexHullShape::wrap_GetPoint(lua_State *state) -> int { return 0; }
auto LuaConvexHullShape::wrap_GetNumFaces(lua_State *state) -> int { return 0; }
auto LuaConvexHullShape::wrap_GetNumVerticesInFace(lua_State *state) -> int { return 0; }
auto LuaConvexHullShape::wrap_GetFaceVertices(lua_State *state) -> int { return 0; }

} // namespace Engine::Physics