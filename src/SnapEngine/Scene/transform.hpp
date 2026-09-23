#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "lua.hpp"

namespace Engine {
struct Transform {
private:
  Math::Vec3 Position;
  Math::Vec3 Scale{1.0F, 1.0F, 1.0F};

  bool LocalDirty = true;
  bool WorldDirty = true;
  bool ChildrenDirty = true;

  Math::Quaternion Rotation;

  Math::Matrix4x4 LocalMatrix;
  Math::Matrix4x4 WorldMatrix;
  Math::Matrix3x3 NormalMatrix;

public:
  [[nodiscard]] auto GetLocalMatrix() const -> const Math::Matrix4x4 & {
    return LocalMatrix;
  }
  [[nodiscard]] auto GetWorldMatrix() const -> const Math::Matrix4x4 & {
    return WorldMatrix;
  }
  [[nodiscard]] auto GetNormalMatrix() const -> const Math::Matrix3x3 & {
    return NormalMatrix;
  }
  [[nodiscard]] auto GetPosition() const -> Math::Vec3 { return Position; }
  [[nodiscard]] auto GetRotation() const -> Math::Quaternion {
    return Rotation;
  }
  [[nodiscard]] auto GetScale() const -> Math::Vec3 { return Scale; }
  auto SetPosition(const Math::Vec3 &position) -> void {
    Position = position;
    LocalDirty = true;
  }
  auto SetRotation(const Math::Quaternion &rotation) -> void {
    Rotation = rotation;
    LocalDirty = true;
  }
  auto SetScale(const Math::Vec3 &scale) -> void {
    Scale = scale;
    LocalDirty = true;
  }

  auto SetPosition(float xPos, float yPos, float zPos) -> void {
    SetPosition(Math::Vec3(xPos, yPos, zPos));
  }

  auto SetRotation(float xRot, float yRot, float zRot, float wRot) -> void {
    SetRotation(Math::Quaternion(xRot, yRot, zRot, wRot));
  }

  auto SetScale(float xScale, float yScale, float zScale) -> void {
    SetScale(Math::Vec3(xScale, yScale, zScale));
  }

  auto ApplyTranslation(const Math::Vec3 &translation) -> void {
    Position += translation;
    LocalDirty = true;
  }

  auto ApplyRotation(const Math::Quaternion &rotation) -> void {
    Rotation = rotation * Rotation;
    LocalDirty = true;
  }

  auto ApplyScaling(const Math::Vec3 &scale) -> void {
    Scale *= scale;
    LocalDirty = true;
  }

  auto UpdateLocalMatrix() -> void;
  auto UpdateWorldMatrix(const Transform *parent) -> void;
  auto MarkChildrenUpdated() -> void;

  constexpr explicit Transform(Math::Vec3 position = {},
                               Math::Quaternion rotation = {0.0F, 0.0F, 0.0F,
                                                            1.0F},
                               Math::Vec3 scale = {1.0F, 1.0F, 1.0F})
      : Position(position), Rotation(rotation), Scale(scale) {}

  constexpr explicit Transform(Math::Quaternion rotation)
      : Position(Math::Vec3{0.0F, 0.0F, 0.0F}), Rotation(rotation),
        Scale(Math::Vec3{1.0F, 1.0F, 1.0F}) {}

  auto DrawGUI(flecs::entity entity) -> void;
};

const static Type TransformType = Type("Transform");

struct LuaTransform : LuaWrap::LuaECSObject {
  static auto GetType() -> const Type * { return &TransformType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override {
    return LuaTransform::GetType();
  }

  static auto SetPosition(lua_State *state) -> int;
  static auto GetPosition(lua_State *state) -> int;
  static auto SetRotation(lua_State *state) -> int;
  static auto GetRotation(lua_State *state) -> int;
  static auto SetScale(lua_State *state) -> int;
  static auto GetScale(lua_State *state) -> int;
  static auto SetTransform(lua_State *state) -> int;
  static auto GetTransform(lua_State *state) -> int;
  static auto GetLocalMatrix(lua_State *state) -> int;
  static auto GetWorldMatrix(lua_State *state) -> int;
  static auto GetUp(lua_State *state) -> int;
  static auto GetRight(lua_State *state) -> int;
  static auto GetForward(lua_State *state) -> int;
  static auto GetInverseUp(lua_State *state) -> int;
  static auto GetInverseRight(lua_State *state) -> int;
  static auto GetInverseForward(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaComponent TransformComponent;

} // namespace Engine