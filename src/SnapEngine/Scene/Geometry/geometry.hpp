#pragma once

#include "Graphics/mesh.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <cstdint>
#include <flecs.h>
#include <lua.hpp>
namespace Engine {

static const Type geometryType = Type("Geometry");

struct Geometry { // NOLINT
  Ref<Graphics::Mesh> mesh;
  uint32_t tlasIndex = 0;
  bool hasTlasIndex = false;
  uint32_t transformIndex = UINT32_MAX;
  uint32_t meshIndex = UINT32_MAX;

  // vertex offset, vertex count per lod
  Math::StackVector<Math::Uvec2, 8> DrawRanges;

  // min area on screen until it goes to a lower lod
  Math::StackVector<float, 8> LodAreaRanges;

  // No clue how to map vertex id -> mesh id currently

  bool dirty = true;

  auto DrawGUI(flecs::entity entity) const -> void;

  ~Geometry();
};

struct LuaGeometry : LuaWrap::LuaECSObject {
  explicit LuaGeometry(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaGeometry> { return Ref<LuaGeometry>::Make(entity); }

  static auto GetType() -> const Type * { return &geometryType; }
  auto GetInstanceType() const -> const Type * override { return &geometryType; }

  static auto GetMesh(lua_State *state) -> int;
  static auto Create(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaComponent GeometryComponent;

} // namespace Engine