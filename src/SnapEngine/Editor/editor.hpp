#pragma once

#include "Graphics/buffer.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/ray.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include <flecs.h>
#include <optional>
#include <queue>
#include <vector>
namespace Engine {

enum class MatchType : uint8_t {
  This,
  Child,
  None,
};

auto DrawEntity(const flecs::entity &entity) -> bool;
auto DrawEntityHierarchy(const flecs::entity &entity, std::string_view filter)
    -> void;
auto DrawSceneHierarchy(const Ref<Engine::Scene> &scene) -> Error;
auto EntityName(const flecs::entity &entity) -> std::string_view;

struct PickEntityReadback {
  Ref<Graphics::Buffer> Buffer;
  Ref<Graphics::BufferReadback> Readback;
};

struct PickEntityResult {
  uint32_t PrimitiveID{};
  uint32_t InstanceID{};
};

enum class TransformAxis : uint8_t {
  None,
  X,
  Y,
  Z,
  XY,
  XZ,
  YZ,
  XYZ,
};

enum class TransformMode : uint8_t {
  None,
  Translate,
  Rotate,
  Scale,
};

struct MoveData {
  std::vector<Transform *> Transforms;
  Math::Vec3 Origin;

  TransformAxis Axis = TransformAxis::None;
  TransformMode Mode = TransformMode::None;

  Math::Ray OriginalRay;
  float OriginalDistance = 0.0F;
  float Distance{};

  Math::Vec2 StartMousePosition;
  Math::Vec2 CurrentMousePosition;

  bool UpdatedTransformAxis;
  bool StartedTransforming;

  Math::Vec3 CurrentTranslation;
  Math::Quaternion CurrentRotation;
  Math::Vec3 CurrentScale;

  auto Update() -> void;
  auto Apply() -> void;
};

struct Editor {
  static std::queue<PickEntityReadback> PickedEntities;
  static MoveData MoveInfo;

  static flecs::entity SelectedEntity;
  static std::vector<flecs::entity> SelectedEntities;
  static flecs::entity EditorCamera;

  static void GizmoTranslation(const Math::Ray &currentRay,
                               Transform &transform);
  static void GizmoRotation(const Math::Ray &currentRay, Transform &transform);
  static void GizmoScale(Transform &transform);
  static void TransformGizmo(const Math::Ray &currentRay, Transform &transform);
  static auto DrawGizmo() -> void;

  // Mouse position is expected to be within the range [0, 1]
  static auto PickEntity(const Graphics::GraphicsContext &context,
                         Math::Vec2 mousePos) -> Error;

  static auto PopEntityPickResult() -> Result<std::optional<PickEntityResult>>;

  static auto StartTranslating() -> void;
  static auto StartRotating() -> void;
  static auto StartScaling() -> void;
  static auto SetTransformAxis(TransformAxis axis) -> void;
  static auto FinalizeTransform() -> void;
};

} // namespace Engine