#include "cameraMatrices.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "frustum.hpp"

namespace Engine {

auto CameraMatrices::GetFrustum() const -> Frustum { return Frustum::FromMatrices(ViewProjectionMatrix, InverseViewProjectionMatrix); }

auto CameraMatrices::Update() -> void {
  constexpr float kUvToNdcScale = 2.0F;

  auto jitteredProjection = ProjectionMatrix;
  // Jitter is stored in UV space; clip-space/NDC offset is twice that range.
  jitteredProjection.At(0, 2) += Jitter.x * kUvToNdcScale;
  jitteredProjection.At(1, 2) += Jitter.y * kUvToNdcScale;

  ViewProjectionMatrix = jitteredProjection * ViewMatrix;
  InverseViewProjectionMatrix = ViewProjectionMatrix.InverseTranspose();
  RotationProjectionMatrix = jitteredProjection * RotationMatrix;
  InverseRotationProjectionMatrix = RotationProjectionMatrix.InverseTranspose();
  InverseRotationMatrix = RotationMatrix.Transpose();
}

auto LuaCameraMatrices::GetRotationMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->RotationMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetInverseRotationMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseRotationMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetViewMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->ViewMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetInverseViewMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseViewMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetProjectionMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->ProjectionMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetInverseProjectionMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseProjectionMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetViewProjectionMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->ViewProjectionMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetInverseViewProjectionMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseViewProjectionMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetRotationProjectionMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->RotationProjectionMatrix.ToLua(state);
}

auto LuaCameraMatrices::GetInverseRotationProjectionMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *cameraMatrices = entity->try_get<CameraMatrices>();
  if (cameraMatrices == nullptr) {
    return luaL_error(state, "Entity does not have a CameraMatrices component");
  }
  return cameraMatrices->InverseRotationProjectionMatrix.ToLua(state);
}

const ::LuaWrap::LuaComponent CameraMatricesComponent{{
    {"getRotationMatrix", LuaCameraMatrices::GetRotationMatrix},
    {"getInverseRotationMatrix", LuaCameraMatrices::GetInverseRotationMatrix},
    {"getViewMatrix", LuaCameraMatrices::GetViewMatrix},
    {"getInverseViewMatrix", LuaCameraMatrices::GetInverseViewMatrix},
    {"getProjectionMatrix", LuaCameraMatrices::GetProjectionMatrix},
    {"getInverseProjectionMatrix", LuaCameraMatrices::GetInverseProjectionMatrix},
    {"getViewProjectionMatrix", LuaCameraMatrices::GetViewProjectionMatrix},
    {"getInverseViewProjectionMatrix", LuaCameraMatrices::GetInverseViewProjectionMatrix},
    {"getRotationProjectionMatrix", LuaCameraMatrices::GetRotationProjectionMatrix},
    {"getInverseRotationProjectionMatrix", LuaCameraMatrices::GetInverseRotationProjectionMatrix},
}};

} // namespace Engine