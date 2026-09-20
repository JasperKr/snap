#include "transform.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/mathTypes.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include "flecs/addons/cpp/c_types.hpp"
#include <imgui.h>
#include <lua.h>
#include <lua.hpp>

namespace Engine {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

auto LuaTransform::SetPosition(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 4) {
    return luaL_error(state, "Expected 3 arguments for position (x, y, z)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->SetPosition(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                         luaL_checkscalar(state, 4));

  return 0;
}

auto LuaTransform::GetPosition(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto position = transform->GetPosition();
  lua_pushnumber(state, position.x);
  lua_pushnumber(state, position.y);
  lua_pushnumber(state, position.z);
  return 3;
}

auto LuaTransform::SetRotation(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 5) {
    return luaL_error(state, "Expected 4 arguments for position (x, y, z, w)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->SetRotation(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                         luaL_checkscalar(state, 4),
                         luaL_checkscalar(state, 5));

  return 0;
}

auto LuaTransform::GetRotation(lua_State *state) -> int {
  // Todo: Type is not Entity, but Lua<...>, LuaCamera for example, which contains an entity field.
  // Need a way to abstract this, so we can get the entity regardless of the specific Lua wrapper type.
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto rotation = transform->GetRotation();
  lua_pushnumber(state, rotation.x);
  lua_pushnumber(state, rotation.y);
  lua_pushnumber(state, rotation.z);
  lua_pushnumber(state, rotation.w);

  return 4;
}

auto LuaTransform::SetScale(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 4) {
    return luaL_error(state, "Expected 3 arguments for position (x, y, z)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->SetScale(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                      luaL_checkscalar(state, 4));

  return 0;
}

auto LuaTransform::GetScale(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto scale = transform->GetScale();
  lua_pushnumber(state, scale.x);
  lua_pushnumber(state, scale.y);
  lua_pushnumber(state, scale.z);

  return 3;
}

auto LuaTransform::SetTransform(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  if (lua_gettop(state) < 11) {
    return luaL_error(state, "Expected 10 arguments for transform (x, y, z, "
                             "rotX, rotY, rotZ, rotW, scaleX, scaleY, scaleZ)");
  }

  auto *transform = entity->try_get_mut<Transform>();
  if (transform == nullptr) {
    entity->add<Transform>();
    transform = entity->try_get_mut<Transform>();
  }

  transform->SetPosition(luaL_checkscalar(state, 2), luaL_checkscalar(state, 3),
                         luaL_checkscalar(state, 4));

  transform->SetRotation(luaL_checkscalar(state, 5), luaL_checkscalar(state, 6),
                         luaL_checkscalar(state, 7),

                         luaL_checkscalar(state, 8));
  transform->SetScale(luaL_checkscalar(state, 9), luaL_checkscalar(state, 10),
                      luaL_checkscalar(state, 11));

  return 0;
}

auto LuaTransform::GetTransform(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto position = transform->GetPosition();
  auto rotation = transform->GetRotation();
  auto scale = transform->GetScale();

  lua_pushnumber(state, position.x);
  lua_pushnumber(state, position.y);
  lua_pushnumber(state, position.z);

  lua_pushnumber(state, rotation.x);
  lua_pushnumber(state, rotation.y);
  lua_pushnumber(state, rotation.z);
  lua_pushnumber(state, rotation.w);

  lua_pushnumber(state, scale.x);
  lua_pushnumber(state, scale.y);
  lua_pushnumber(state, scale.z);

  return 10;
}

auto LuaTransform::GetLocalMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  return transform->GetLocalMatrix().ToLua(state);
}

auto LuaTransform::GetWorldMatrix(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  return transform->GetWorldMatrix().ToLua(state);
}

auto Transform::UpdateLocalMatrix() -> void {
  if (!LocalDirty) {
    return;
  }

  LocalMatrix =
      Math::Matrix4x4::TransformationMatrix(Position, Scale, Rotation);
  LocalDirty = false;
  WorldDirty = true;
}

auto Transform::UpdateWorldMatrix(const Transform *parent) -> void {
  if (parent != nullptr && parent->ChildrenDirty) {
    WorldDirty = true;
  }

  if (!WorldDirty) {
    return;
  }

  if (parent != nullptr) {
    WorldMatrix = parent->WorldMatrix * LocalMatrix;
  } else {
    WorldMatrix = LocalMatrix;
  }

  NormalMatrix = Math::Matrix3x3(WorldMatrix).InverseTranspose();

  WorldDirty = false;
  ChildrenDirty = true;
}

auto Transform::MarkChildrenUpdated() -> void { ChildrenDirty = false; }

enum class RotationMode : uint8_t {
  EulerRadians,
  EulerDegrees,
  Quaternion,
};

auto Transform::DrawGUI(flecs::entity entity) -> void {
  ImGuiDataType dataType = sizeof(float) == sizeof(double)
                               ? ImGuiDataType_Double
                               : ImGuiDataType_Float;
  if (ImGui::DragScalarN("Position", dataType, (void *)Position.Ptr(), 3,
                         0.1F)) {
    LocalDirty = true;
  }

  static RotationMode rotationMode = RotationMode::EulerDegrees;
  static flecs::id_t lastEntity = 0;
  static Math::EulerAngle lastEulerRotation;
  static Math::Quaternion lastQuaternionRotation;
  static bool UserInteractedLastFrame = false;

  if (ImGui::BeginCombo("Rotation Mode", [&]() -> const char * {
        switch (rotationMode) {
        case RotationMode::EulerRadians:
          return "Euler Radians";
        case RotationMode::EulerDegrees:
          return "Euler Degrees";
        case RotationMode::Quaternion:
          return "Quaternion";
        default:
          return "Unknown";
        }
      }())) {
    if (ImGui::Selectable("Euler Radians")) {
      rotationMode = RotationMode::EulerRadians;
    }
    if (ImGui::Selectable("Euler Degrees")) {
      rotationMode = RotationMode::EulerDegrees;
    }
    if (ImGui::Selectable("Quaternion")) {
      rotationMode = RotationMode::Quaternion;
    }
    ImGui::EndCombo();
  }

  Rotation = Rotation.Normalize();

  // Load rotation from entity, if it has changed
  if (lastEntity != entity.id()) {
    lastEntity = entity.id();
    lastEulerRotation = Math::Conversions::ToEuler(Rotation).ToDegrees();
    lastQuaternionRotation = Rotation;
  }

  // Keep up to date if, for example, physics is updating the rotation
  if (Rotation != lastQuaternionRotation && !UserInteractedLastFrame) {
    lastQuaternionRotation = Rotation;
    lastEulerRotation = Math::Conversions::ToEuler(Rotation).ToDegrees();
  }

  UserInteractedLastFrame = false;

  switch (rotationMode) {
  case RotationMode::EulerRadians: {
    if (ImGui::DragScalarN("Euler Rotation", dataType,
                           (void *)lastEulerRotation.Ptr(), 3, 0.01F)) {
      lastEulerRotation.SanitiseAsRadians();
      Rotation = Math::Conversions::ToQuaternion(lastEulerRotation);

      // Make sure we don't trigger the if-statement above on the next frame
      lastQuaternionRotation = Rotation;
      LocalDirty = true;
    }
    break;
  }
  case RotationMode::EulerDegrees: {
    if (ImGui::DragScalarN("Euler Rotation", dataType,
                           (void *)lastEulerRotation.Ptr(), 3, 1.0F)) {
      lastEulerRotation.SanitiseAsDegrees();
      Rotation = Math::Conversions::ToQuaternion(lastEulerRotation.ToRadians());

      // Make sure we don't trigger the if-statement above on the next frame
      lastQuaternionRotation = Rotation;
      LocalDirty = true;
    }
    break;
  }
  case RotationMode::Quaternion:
    if (ImGui::DragScalarN("Quaternion Rotation", dataType,
                           (void *)Rotation.Ptr(), 4, 0.01F)) {
      LocalDirty = true;
      lastQuaternionRotation = Rotation;
    }
    break;
  }

  if (ImGui::IsItemActivated() || ImGui::IsItemDeactivatedAfterEdit() ||
      ImGui::IsItemActive()) {
    UserInteractedLastFrame = true;
  }

  if (ImGui::DragScalarN("Scale", dataType, (void *)Scale.Ptr(), 3, 0.1F)) {
    LocalDirty = true;
  }

  if (ImGui::TreeNode("Local Matrix")) {
    ImGui::Text("%s", LocalMatrix.ToString().c_str());
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("World Matrix")) {
    ImGui::Text("%s", WorldMatrix.ToString().c_str());
    ImGui::TreePop();
  }
}

auto LuaTransform::GetUp(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec = transform->GetRotation().RotateVector({0.0F, 1.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetRight(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec = transform->GetRotation().RotateVector({1.0F, 0.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetForward(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec = transform->GetRotation().RotateVector({0.0F, 0.0F, 1.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetInverseUp(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec =
      transform->GetRotation().Inverse().RotateVector({0.0F, 1.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetInverseRight(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec =
      transform->GetRotation().Inverse().RotateVector({1.0F, 0.0F, 0.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

auto LuaTransform::GetInverseForward(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity object");
  }

  const auto *transform = entity->try_get<Transform>();
  if (transform == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  auto vec =
      transform->GetRotation().Inverse().RotateVector({0.0F, 0.0F, 1.0F});
  lua_pushnumber(state, vec.x);
  lua_pushnumber(state, vec.y);
  lua_pushnumber(state, vec.z);

  return 3;
}

extern const ::LuaWrap::LuaComponent TransformComponent{{
    {"setPosition", LuaTransform::SetPosition},
    {"getPosition", LuaTransform::GetPosition},
    {"setRotation", LuaTransform::SetRotation},
    {"getRotation", LuaTransform::GetRotation},
    {"setScale", LuaTransform::SetScale},
    {"getScale", LuaTransform::GetScale},
    {"setTransform", LuaTransform::SetTransform},
    {"getTransform", LuaTransform::GetTransform},
    {"getLocalMatrix", LuaTransform::GetLocalMatrix},
    {"getWorldMatrix", LuaTransform::GetWorldMatrix},
    {"getUp", LuaTransform::GetUp},
    {"getRight", LuaTransform::GetRight},
    {"getForward", LuaTransform::GetForward},
    {"getInverseUp", LuaTransform::GetInverseUp},
    {"getInverseRight", LuaTransform::GetInverseRight},
    {"getInverseForward", LuaTransform::GetInverseForward},
}};

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
} // namespace Engine