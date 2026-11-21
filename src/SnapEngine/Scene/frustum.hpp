#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Scene/Geometry/boundingBox.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <array>
#include <cassert>
#include <lua.hpp>
namespace Engine {

struct Frustum {
  constexpr static size_t CornerCount = 8;
  constexpr static size_t PlaneCount = 6;

  // Frustum planes are represented as Vec4s
  // Plane is normal + distance, where the normal is normalized and points inward
  Math::Plane Near;
  Math::Plane Far;
  Math::Plane Left;
  Math::Plane Right;
  Math::Plane Top;
  Math::Plane Bottom;

  // std::array<Math::Vec3, CornerCount> Corners{};

  // Returns the corners of the frustum in the following order:
  // NTL, NTR, NBR, NBL, FTL, FTR, FBR, FBL
  // [[nodiscard]] auto GetCorners() const -> std::array<Math::Vec3, CornerCount> {
  //   return Corners;
  // };

  constexpr static size_t NTL = 0;
  constexpr static size_t NTR = 1;
  constexpr static size_t NBR = 2;
  constexpr static size_t NBL = 3;
  constexpr static size_t FTL = 4;
  constexpr static size_t FTR = 5;
  constexpr static size_t FBR = 6;
  constexpr static size_t FBL = 7;

  // [[nodiscard]] auto GetCorner(size_t index) const -> Math::Vec3 {
  //   assert(index < CornerCount && "Frustum corner index out of range");
  //   assert(index >= 0 && "Frustum corner index out of range");
  //   return Corners.at(index);
  // }

  // Returns the planes of the frustum in the following order:
  // Near, Far, Left, Right, Top, Bottom
  [[nodiscard]] auto GetPlanes() const -> std::array<Math::Plane, PlaneCount> { return {Near, Far, Left, Right, Top, Bottom}; }

  [[nodiscard]] auto GetPlane(size_t index) -> Math::Plane &;

  [[nodiscard]] auto GetPlane(size_t index) const -> const Math::Plane &;

  // Returns true if the point is inside the frustum
  [[nodiscard]] auto ContainsPoint(const Math::Vec3 &point) const -> bool;

  [[nodiscard]] auto IntersectsSphere(const Math::Vec3 &center, float radius) const -> bool;

  [[nodiscard]] auto IntersectsAABB(const Math::Vec3 &min, const Math::Vec3 &max, bool precise = true) const -> bool;

  [[nodiscard]] auto IntersectsAABB(const BoundingBox &box, bool precise = true) const -> bool { return IntersectsAABB(box.Min, box.Max, precise); }

  static auto FromMatrices(const Math::Matrix4x4 &viewProjectionMatrix, const Math::Matrix4x4 &inverseViewProjectionMatrix) -> Frustum;
};

static const Type FrustumType = Type("Frustum");

struct LuaFrustum : LuaWrap::LuaECSObject {
  static auto GetType() -> const Type * { return &FrustumType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override { return LuaFrustum::GetType(); }

  static auto GetNearPlane(lua_State *state) -> int;
  static auto GetFarPlane(lua_State *state) -> int;
  static auto GetLeftPlane(lua_State *state) -> int;
  static auto GetRightPlane(lua_State *state) -> int;
  static auto GetTopPlane(lua_State *state) -> int;
  static auto GetBottomPlane(lua_State *state) -> int;

  static auto GetCorner(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaComponent FrustumComponent;

} // namespace Engine