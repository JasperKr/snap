#pragma once

#include "Modules/Math/matrix.hpp"
#include "Modules/Math/vector.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
namespace Engine {

struct CameraMatrices {
  Math::Matrix4x4 RotationMatrix;
  Math::Matrix4x4 InverseRotationMatrix;
  Math::Matrix4x4 ViewMatrix;
  Math::Matrix4x4 InverseViewMatrix;
  Math::Matrix4x4 ProjectionMatrix;
  Math::Matrix4x4 InverseProjectionMatrix;
  Math::Matrix4x4 ViewProjectionMatrix;
  Math::Matrix4x4 InverseViewProjectionMatrix;
  Math::Matrix4x4 RotationProjectionMatrix;
  Math::Matrix4x4 InverseRotationProjectionMatrix;
  Math::Vec2 Jitter;

  [[nodiscard]] auto GetForward() const -> Math::Vec3 {
    return Math::Vec3{-ViewMatrix.At(2, 0), -ViewMatrix.At(2, 1),
                      -ViewMatrix.At(2, 2)};
  }

  [[nodiscard]] auto GetUp() const -> Math::Vec3 {
    return Math::Vec3{ViewMatrix.At(1, 0), ViewMatrix.At(1, 1),
                      ViewMatrix.At(1, 2)};
  }

  [[nodiscard]] auto GetRight() const -> Math::Vec3 {
    return Math::Vec3{ViewMatrix.At(0, 0), ViewMatrix.At(0, 1),
                      ViewMatrix.At(0, 2)};
  }

  [[nodiscard]] auto GetPosition() const -> Math::Vec3 {
    return Math::Vec3{InverseViewMatrix.At(3, 0), InverseViewMatrix.At(3, 1),
                      InverseViewMatrix.At(3, 2)};
  }

  [[nodiscard]] auto Project(Math::Vec3 position) const -> Math::Vec3 {
    Math::Vec4 clip = ViewProjectionMatrix * Math::Vec4(position, 1.0F);
    clip /= clip.w;
    clip *= 0.5F;
    clip += 0.5F;
    return Math::Vec3(clip);
  }

  [[nodiscard]] auto InverseProject(Math::Vec3 uvs) const -> Math::Vec3 {
    uvs *= 2.0F;
    uvs -= 1.0F;
    Math::Vec4 world = InverseViewProjectionMatrix * Math::Vec4(uvs, 1.0F);
    world /= world.w;
    return Math::Vec3(world);
  }

  [[nodiscard]] auto InverseProject_Rotation(Math::Vec3 uvs) const
      -> Math::Vec3 {
    uvs *= 2.0F;
    uvs -= 1.0F;
    Math::Vec4 world = InverseRotationProjectionMatrix * Math::Vec4(uvs, 1.0F);
    world /= world.w;
    return Math::Vec3(world);
  }

  [[nodiscard]] auto GetFrustum() const -> struct Frustum;
  auto Update() -> void;
};

const Type cameraMatricesType = Type("CameraMatrices");

struct LuaCameraMatrices : LuaWrap::LuaECSObject {
  explicit LuaCameraMatrices(const flecs::entity &entity)
      : LuaECSObject(entity) {}

  static auto GetType() -> const Type * { return &cameraMatricesType; }
  auto GetInstanceType() const -> const Type * override {
    return &cameraMatricesType;
  }

  static auto FromEntity(const flecs::entity &entity)
      -> Ref<LuaCameraMatrices> {
    return Ref<LuaCameraMatrices>::Make(entity);
  }

  static auto GetRotationMatrix(lua_State *state) -> int;
  static auto GetInverseRotationMatrix(lua_State *state) -> int;
  static auto GetViewMatrix(lua_State *state) -> int;
  static auto GetInverseViewMatrix(lua_State *state) -> int;
  static auto GetProjectionMatrix(lua_State *state) -> int;
  static auto GetInverseProjectionMatrix(lua_State *state) -> int;
  static auto GetViewProjectionMatrix(lua_State *state) -> int;
  static auto GetInverseViewProjectionMatrix(lua_State *state) -> int;
  static auto GetRotationProjectionMatrix(lua_State *state) -> int;
  static auto GetInverseRotationProjectionMatrix(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaComponent CameraMatricesComponent;

} // namespace Engine