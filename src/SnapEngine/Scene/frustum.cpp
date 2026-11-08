#include "frustum.hpp"
#include "Modules/Math/vector.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"

namespace Engine {

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Frustum::IntersectsAABB(const Math::Vec3 &min, const Math::Vec3 &max, bool precise) const -> bool {
  for (int i = 0; i < PlaneCount; i++) {
    int out = 0;
    auto plane = GetPlane(i);

    out += plane.Dot(min) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, min.y, min.z)) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, max.y, min.z)) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, max.y, min.z)) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, min.y, max.z)) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(max.x, min.y, max.z)) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(Math::Vec3(min.x, max.y, max.z)) + plane.w < 0.0F ? 1 : 0;
    out += plane.Dot(max) + plane.w < 0.0F ? 1 : 0;

    if (out == 8) { // NOLINT
      return false;
    }
  }

  // if (precise) {
  //   int out = 0;
  //   // clang-format off
  //   // NOLINTBEGIN
  //   out = 0; for (const auto &corner : Corners) { out += corner.x > max.x ? 1 : 0; }; if (out == 8) { return false; }
  //   out = 0; for (const auto &corner : Corners) { out += corner.x < min.x ? 1 : 0; }; if (out == 8) { return false; }
  //   out = 0; for (const auto &corner : Corners) { out += corner.y > max.y ? 1 : 0; }; if (out == 8) { return false; }
  //   out = 0; for (const auto &corner : Corners) { out += corner.y < min.y ? 1 : 0; }; if (out == 8) { return false; }
  //   out = 0; for (const auto &corner : Corners) { out += corner.z > max.z ? 1 : 0; }; if (out == 8) { return false; }
  //   out = 0; for (const auto &corner : Corners) { out += corner.z < min.z ? 1 : 0; }; if (out == 8) { return false; }
  //   // NOLINTEND
  //   // clang-format on
  // }

  return true;
}

auto Frustum::FromMatrices(const Math::Matrix4x4 &viewProjectionMatrix, const Math::Matrix4x4 &inverseViewProjectionMatrix) -> Frustum {
  Frustum frustum;

  thread_local std::array<Math::Vec4, CornerCount> ndc_corners = {
      Math::Vec4(-1.0F, -1.0F, -1.0F, 1.0F), // NTL
      Math::Vec4(1.0F, -1.0F, -1.0F, 1.0F),  // NTR
      Math::Vec4(1.0F, -1.0F, 1.0F, 1.0F),   // NBR
      Math::Vec4(-1.0F, -1.0F, 1.0F, 1.0F),  // NBL
      Math::Vec4(-1.0F, 1.0F, -1.0F, 1.0F),  // FTL
      Math::Vec4(1.0F, 1.0F, -1.0F, 1.0F),   // FTR
      Math::Vec4(1.0F, 1.0F, 1.0F, 1.0F),    // FBR
      Math::Vec4(-1.0F, 1.0F, 1.0F, 1.0F)    // FBL
  };

  // for (size_t i = 0; i < CornerCount; ++i) {
  //   auto ndc_corner = ndc_corners.at(i);
  //   ndc_corner.z *= -1.0F; // Reverse Z
  //   Math::Vec4 world_pos = inverseViewProjectionMatrix * ndc_corner;
  //   frustum.Corners.at(i) = Math::Vec3{world_pos / world_pos.w};
  // }

  // Gribb/Hartmann method for extracting planes from the view-projection matrix
  const auto &mat = viewProjectionMatrix;

  for (int i = 0; i < 4; ++i) {
    frustum.Left[i] = mat.At(3, i) + mat.At(0, i);
    frustum.Right[i] = mat.At(3, i) - mat.At(0, i);
    frustum.Bottom[i] = mat.At(3, i) + mat.At(1, i);
    frustum.Top[i] = mat.At(3, i) - mat.At(1, i);
    frustum.Near[i] = mat.At(3, i) + mat.At(2, i);
    frustum.Far[i] = mat.At(3, i) - mat.At(2, i);
  }

  // Normalize the planes
  frustum.Left = frustum.Left.Normalize();
  frustum.Right = frustum.Right.Normalize();
  frustum.Bottom = frustum.Bottom.Normalize();
  frustum.Top = frustum.Top.Normalize();
  frustum.Near = frustum.Near.Normalize();
  frustum.Far = frustum.Far.Normalize();

  return frustum;
}

auto LuaFrustum::GetNearPlane(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Near.x);
  lua_pushnumber(state, frustum->Near.y);
  lua_pushnumber(state, frustum->Near.z);
  lua_pushnumber(state, frustum->Near.w);

  return 4;
};

auto LuaFrustum::GetFarPlane(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Far.x);
  lua_pushnumber(state, frustum->Far.y);
  lua_pushnumber(state, frustum->Far.z);
  lua_pushnumber(state, frustum->Far.w);

  return 4;
};

auto LuaFrustum::GetLeftPlane(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Left.x);
  lua_pushnumber(state, frustum->Left.y);
  lua_pushnumber(state, frustum->Left.z);
  lua_pushnumber(state, frustum->Left.w);

  return 4;
};

auto LuaFrustum::GetRightPlane(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Right.x);
  lua_pushnumber(state, frustum->Right.y);
  lua_pushnumber(state, frustum->Right.z);
  lua_pushnumber(state, frustum->Right.w);

  return 4;
};

auto LuaFrustum::GetTopPlane(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Top.x);
  lua_pushnumber(state, frustum->Top.y);
  lua_pushnumber(state, frustum->Top.z);
  lua_pushnumber(state, frustum->Top.w);

  return 4;
};

auto LuaFrustum::GetBottomPlane(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  lua_pushnumber(state, frustum->Bottom.x);
  lua_pushnumber(state, frustum->Bottom.y);
  lua_pushnumber(state, frustum->Bottom.z);
  lua_pushnumber(state, frustum->Bottom.w);

  return 4;
};

auto LuaFrustum::GetCorner(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  const auto *frustum = entity->try_get<Frustum>();
  if (frustum == nullptr) {
    return luaL_error(state, "Entity does not have a Frustum component");
  }

  int index = static_cast<int>(luaL_checkinteger(state, 2));
  if (index < 0 || index >= Frustum::CornerCount) {
    return luaL_error(state, "Corner index out of range");
  }

  // const auto &corner = frustum->Corners.at(static_cast<size_t>(index));
  // lua_pushnumber(state, corner.x);
  // lua_pushnumber(state, corner.y);
  // lua_pushnumber(state, corner.z);

  lua_pushnumber(state, 0);
  lua_pushnumber(state, 0);
  lua_pushnumber(state, 0);

  return 3;
};

const ::LuaWrap::LuaComponent FrustumComponent{{
    {"getNearPlane", LuaFrustum::GetNearPlane},
    {"getFarPlane", LuaFrustum::GetFarPlane},
    {"getLeftPlane", LuaFrustum::GetLeftPlane},
    {"getRightPlane", LuaFrustum::GetRightPlane},
    {"getTopPlane", LuaFrustum::GetTopPlane},
    {"getBottomPlane", LuaFrustum::GetBottomPlane},
    {"getCorner", LuaFrustum::GetCorner},
}};

} // namespace Engine