#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>
namespace Engine {

static const Type geometryType = Type("Geometry");

struct Geometry {
  Ref<Graphics::Mesh> mesh;
  uint32_t tlasIndex = 0;
  bool hasTlasIndex = false;

  auto DrawGUI(flecs::entity entity) const -> void;
};

struct LuaGeometry : LuaWrap::LuaECSObject {
  explicit LuaGeometry(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaGeometry> { return Ref<LuaGeometry>::Make(entity); }

  static auto GetType() -> const Type * { return &geometryType; }
  auto GetInstanceType() const -> const Type * override { return &geometryType; }

  static auto GetMesh(lua_State *state) -> int;
  static auto Create(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaClass GeometryClass;

} // namespace Engine