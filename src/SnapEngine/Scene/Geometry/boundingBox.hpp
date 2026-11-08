#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/transform.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <flecs.h>
#include <imgui.h>
#include <lua.hpp>
namespace Engine {
struct BoundingBox {
  Math::Vec3 Min;
  Math::Vec3 Max;

  [[nodiscard]] auto GetCenter() const -> Math::Vec3;
  [[nodiscard]] auto GetSize() const -> Math::Vec3;
  [[nodiscard]] auto Union(const BoundingBox &other) const -> BoundingBox;
  auto Union(const BoundingBox &other, BoundingBox &result) const -> void;
  auto UnionInPlace(const BoundingBox &other) -> void;
  [[nodiscard]] auto Intersect(const BoundingBox &other) const -> BoundingBox;
  auto Intersect(const BoundingBox &other, BoundingBox &result) const -> void;
  auto IntersectInPlace(const BoundingBox &other) -> void;
  [[nodiscard]] auto IsValid() const -> bool;
  auto Grow(const Math::Vec3 &point) -> void;
  auto Reset() -> void {
    Min = Math::Vec3(std::numeric_limits<float>::max());
    Max = Math::Vec3(std::numeric_limits<float>::lowest());
  }

  auto Construct(const Transform &transform, const BoundingBox &localBounds) -> void;

  auto DrawGUI(flecs::entity entity) const -> void;
};

static const Type boundingBoxType = Type("BoundingBox");

struct LuaBoundingBox : LuaWrap::LuaECSObject {
  explicit LuaBoundingBox(const flecs::entity &entity) : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &boundingBoxType; }
  auto GetInstanceType() const -> const Type * override { return &boundingBoxType; }

  static auto FromEntity(const flecs::entity &entity) -> Ref<LuaBoundingBox> { return Ref<LuaBoundingBox>::Make(entity); }

  static auto GetMin(lua_State *state) -> int;
  static auto SetMin(lua_State *state) -> int;
  static auto GetMax(lua_State *state) -> int;
  static auto SetMax(lua_State *state) -> int;

  static auto GetCenter(lua_State *state) -> int;
  static auto GetSize(lua_State *state) -> int;
  static auto Union(lua_State *state) -> int;
  static auto Intersect(lua_State *state) -> int;

  static auto UnionInPlace(lua_State *state) -> int;
  static auto IntersectInPlace(lua_State *state) -> int;
};

struct LocalBounds {
  BoundingBox Bounds;

  auto DrawGUI(flecs::entity entity) const -> void {
    ImGui::TextDisabled("Local Bounds");
    Bounds.DrawGUI(entity);
  }
};
struct WorldBounds {
  BoundingBox Bounds;

  auto DrawGUI(flecs::entity entity) const -> void {
    ImGui::TextDisabled("World Bounds");
    Bounds.DrawGUI(entity);
  }
};

extern const ::LuaWrap::LuaClass BoundingBoxClass;

} // namespace Engine