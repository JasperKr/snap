#include "editor.hpp"
#include "Editor/lineDrawer.hpp"
#include "Graphics/buffer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/ray.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/window.hpp"
#include "Renderer/shaderManager.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Scene/cameraMatrices.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "renderer.hpp"
#include <format>
#include <imgui.h>
#include <lua.h>
#include <mutex>
#include <string>
#include <string_view>

namespace Engine {

std::queue<PickEntityReadback> Engine::Editor::PickedEntities{};
MoveData Engine::Editor::MoveInfo{};

flecs::entity Engine::Editor::SelectedEntity{};
std::vector<flecs::entity> Engine::Editor::SelectedEntities{};
flecs::entity Engine::Editor::EditorCamera{};

auto EntityName(const flecs::entity &entity) -> std::string_view {
  const char *entityName = entity.name();

  if (entityName == nullptr || std::string_view(entityName).empty()) {
    if (entity != flecs::ChildOf) {
      entityName = "Unnamed Component";
    } else {
      entityName = "Unnamed Entity";
    }
  }

  const auto *name = entity.try_get<DisplayName>();
  if (name != nullptr && !name->Name.empty()) {
    entityName = name->Name.c_str();
  }

  return {entityName};
}

auto DrawEntity(const flecs::entity &entity) -> bool {
  auto entityName = EntityName(entity);

  ImGui::PushID(static_cast<int>(entity.id()));

  ImGuiTreeNodeFlags nodeFlags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

  if (Editor::SelectedEntity == entity) {
    // NOLINTNEXTLINE
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  }

  bool node = ImGui::TreeNodeEx(entityName.data(), nodeFlags);
  if (ImGui::IsItemClicked()) {
    Editor::SelectedEntity = entity;
  }

  ImGui::PopID();

  return node;
}

inline auto fuzzyMatch(const std::string_view &pattern,
                       const std::string_view &str) -> bool {
  size_t index = 0;
  for (char character : str) {
    if (index < pattern.size() &&
        tolower(character) == tolower(pattern[index])) {
      ++index;
    }
  }
  return index == pattern.size();
}

inline auto matchesRecursive(const flecs::entity &entity,
                             const std::string_view &filter) -> MatchType {
  if (filter.empty()) {
    return MatchType::This; // If filter is empty, match all entities
  }

  if (fuzzyMatch(filter, std::string_view(entity.name()))) {
    return MatchType::This;
  }

  auto matches = MatchType::None;
  entity.children([&](flecs::entity child) -> void {
    if (matches != MatchType::None) {
      return; // If a match has already been found, skip further checks
    }

    if (matchesRecursive(child, filter) != MatchType::None) {
      matches = MatchType::Child;
    }
  });

  return matches;
}

auto DrawEntityHierarchy(const flecs::entity &entity, std::string_view filter)
    -> void {

  auto match = matchesRecursive(entity, filter);
  if (match == MatchType::None) {
    return; // Skip entities that don't match the filter
  }

  const char *entityName = entity.name();

  if (strcmp(entityName, "flecs") == 0 || strcmp(entityName, "Engine") == 0) {
    return; // Skip internal flecs root entity
  }

  // Skip systems
  if (entity.has(flecs::System)) {
    return;
  }

  bool drawChildren = DrawEntity(entity);

  if (match == MatchType::This) {
    // If this entity matches, all children should be shown regardless of their names
    filter = "";
  }

  if (drawChildren) {
    ImGui::Indent();
    entity.children([&](flecs::entity child) -> void {
      DrawEntityHierarchy(child, filter);
    });
    ImGui::Unindent();
    ImGui::TreePop();
  }
}

inline auto DrawEntityEditor(flecs::entity entity, const Ref<Scene> &scene)
    -> void {
  if (!Editor::SelectedEntity.is_valid()) {
    return;
  }

  ImGui::PushID(static_cast<int>(Editor::SelectedEntity.id()));

  Editor::SelectedEntity.each([&](flecs::id identifier) -> auto {
    if (!identifier.is_entity()) {
      return;
    }

    auto componentEntity = identifier.entity();
    if (componentEntity == flecs::ChildOf || !componentEntity.is_valid()) {
      return;
    }

    auto componentName = EntityName(componentEntity);
    ImGui::TextDisabled("-%s", componentName.data());
  });

  Editor::SelectedEntity.each([&](flecs::id identifier) -> auto {
    if (!identifier.is_entity()) {
      return;
    }

    auto componentEntity = identifier.entity();

    if (componentEntity == flecs::ChildOf || !componentEntity.is_valid()) {
      return;
    }

    auto componentName = EntityName(componentEntity);

    auto iter = scene->drawFunctions.find(componentEntity.id());
    if (iter != scene->drawFunctions.end()) {
      ImGui::Separator();
      ImGui::PushID(static_cast<int>(componentEntity.id()));
      auto name = std::format("Component: {}", componentName.data());
      if (ImGui::TreeNodeEx(name.c_str(), iter->second.defaultOpen
                                              ? ImGuiTreeNodeFlags_DefaultOpen
                                              : 0)) {
        iter->second.func(Editor::SelectedEntity);
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
  });

  ImGui::PopID();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto DrawSceneHierarchy(const Ref<Engine::Scene> &scene) -> Error {
  auto pickReadbackResult = CHECK_RES(Editor::PopEntityPickResult());

  if (pickReadbackResult != std::nullopt) {
    auto pickResult = pickReadbackResult.value();

    scene->world.each<Geometry>(
        [&](flecs::entity entity, const Geometry &geometry) -> void {
          if (geometry.hasTlasIndex &&
              geometry.tlasIndex == pickResult.InstanceID) {
            Editor::SelectedEntity = entity;
            Editor::SelectedEntities = {Editor::SelectedEntity};

            Editor::MoveInfo.UpdatedTransformAxis = true;
          }
        });
  }

  Editor::MoveInfo.Transforms.clear();

  for (const auto entity : Editor::SelectedEntities) {
    Editor::MoveInfo.Transforms.emplace_back(entity.try_get_mut<Transform>());
  }

  Editor::MoveInfo.Update();
  Editor::DrawGizmo();

  static bool DrawBoundingBox = false;
  static bool DrawBoundsRecursively = false;

  ImGui::Begin(scene->name.c_str());

  static char buf[256] = {};                    // NOLINT
  ImGui::InputText("Search", buf, sizeof(buf)); // NOLINT
  ImGui::Checkbox("Draw Bounds", &DrawBoundingBox);
  ImGui::SameLine();
  ImGui::Checkbox("Draw Bounds Recursively", &DrawBoundsRecursively);

  ImGui::Separator();
  ImGui::Text("Entity Hierarchy:");

  static float hierarchyHeightRatio = 0.5F; // NOLINT
  const float separatorHeight = 8.0F;

  auto availableHeight = ImGui::GetContentRegionAvail().y;

  if (ImGui::BeginChild(
          "Entity Hierarchy",
          ImVec2(0, availableHeight * hierarchyHeightRatio), // NOLINT
          ImGuiChildFlags_Borders)) {

    scene->world.entity(0).children([&](flecs::entity entity) -> void {
      DrawEntityHierarchy(entity, std::string_view(buf)); // NOLINT
    });
  }
  ImGui::EndChild();

  auto cursorPos = ImGui::GetCursorPos();

  ImGui::InvisibleButton(" ", ImVec2(ImGui::GetContentRegionAvail().x,
                                     ImGui::GetTextLineHeightWithSpacing()));

  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    hierarchyHeightRatio += ImGui::GetIO().MouseDelta.y / availableHeight;
    hierarchyHeightRatio =
        std::clamp(hierarchyHeightRatio, 0.1F, 0.9F); // NOLINT
  }

  ImGui::SetCursorPos(cursorPos);
  ImGui::SeparatorText("");

  if (ImGui::BeginChild("Selected Entity", ImVec2(0, 0),
                        ImGuiChildFlags_Borders)) {
    DrawEntityEditor(Editor::SelectedEntity, scene);
  }
  ImGui::EndChild();
  ImGui::End();

  return {};
}

auto Editor::PickEntity(const Graphics::GraphicsContext &context,
                        Math::Vec2 mousePos) -> Error {

  auto shader =
      CHECK_RES(Renderer::RendererInstance.GetShaderManager().GetShader(
          Renderer::ShaderKey::ObjectPicker));

  auto &tlas = Renderer::RendererInstance.GetSceneTLAS();

  Graphics::BufferCreationInfo info{
      .size = sizeof(uint32_t) * 4,
      .usage =
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .stagingBuffer = true,
      .persistentMapping = true,
      .debugName = "Entity Pick Buffer",
  };

  auto pickBuffer = CHECK_RES(Graphics::Buffer::Create(context, info));

  const auto &camera = Editor::EditorCamera.get<Camera>();

  CHECK_ERR(shader->Send(Graphics::ResourceKey{"SceneBVH"}, tlas));
  CHECK_ERR(shader->Send(Graphics::ResourceKey{"CameraData"},
                         camera.GetBuffer()->GetBuffer()));
  CHECK_ERR(shader->Send(Graphics::ResourceKey{"TraceResults"}, pickBuffer));

  CHECK_ERR(Graphics::UniformWriter::Send(
      shader, Graphics::ResourceKey{"PushConstants", "uv"}, mousePos));

  Graphics::RenderState::SetShader(shader);

  CHECK_ERR(Graphics::Dispatch(context, {1, 1, 1}));

  auto readback = CHECK_RES(pickBuffer->Readback(context));

  PickEntityReadback pickReadback{
      .Buffer = pickBuffer,
      .Readback = readback,
  };

  PickedEntities.push(pickReadback);

  return {};
}

auto Editor::PopEntityPickResult() -> Result<std::optional<PickEntityResult>> {
  if (PickedEntities.empty()) {
    return std::nullopt;
  }

  auto pickReadback = PickedEntities.front();

  if (!pickReadback.Readback.isValid()) {
    PickedEntities.pop();
    return std::nullopt;
  }

  std::lock_guard<std::mutex> lock(pickReadback.Readback->mutex);

  if (!pickReadback.Readback->completed) {
    return std::nullopt;
  }

  PickedEntities.pop();

  if (Error::IsError(pickReadback.Readback->error)) {
    return pickReadback.Readback->error;
  }

  auto data = pickReadback.Readback->data;

  struct PickResult {
    uint32_t instanceID;
    uint32_t primitiveID;
    uint32_t hit;
    uint32_t padding;
  };

  const auto *pickResult = // NOLINTNEXTLINE
      reinterpret_cast<const PickResult *>(data->GetData());

  if (pickResult->hit == 0) {
    return std::nullopt;
  }

  PickEntityResult pickedEntity;
  pickedEntity.PrimitiveID = pickResult->primitiveID;
  pickedEntity.InstanceID = pickResult->instanceID;

  return pickedEntity;
}

auto GizmoTranslation(const MoveData &moveData, const Math::Ray &currentRay)
    -> Math::Vec3 {
  Math::Vec3 OriginToCamera = (currentRay.Origin - moveData.Origin).Normalize();

  switch (moveData.Axis) {
  case TransformAxis::None:
    return {};
  case TransformAxis::X: // Plane on to X axis, normal = OriginToCamera
  {
    Math::Vec3 normal = OriginToCamera;
    normal.x = 0.0F;
    normal = normal.Normalize();
    Math::Plane plane(moveData.Origin, normal);
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(plane);
    if (!intersection.has_value()) {
      return {};
    }

    Math::Vec3 intersectionPoint = intersection.value();
    Math::Vec3 translation = intersectionPoint - moveData.Origin;
    translation.y = 0.0F;
    translation.z = 0.0F;

    return translation;
  }
  case TransformAxis::Y: // Plane on to Y axis, normal = OriginToCamera
  {
    Math::Vec3 normal = OriginToCamera;
    normal.y = 0.0F;
    normal = normal.Normalize();
    Math::Plane plane(moveData.Origin, normal);
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(plane);
    if (!intersection.has_value()) {
      return {};
    }

    Math::Vec3 intersectionPoint = intersection.value();
    Math::Vec3 translation = intersectionPoint - moveData.Origin;
    translation.x = 0.0F;
    translation.z = 0.0F;

    return translation;
  }
  case TransformAxis::Z: // Plane on to Z axis, normal = OriginToCamera
  {
    Math::Vec3 normal = OriginToCamera;
    normal.z = 0.0F;
    normal = normal.Normalize();
    Math::Plane plane(moveData.Origin, normal);
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(plane);
    if (!intersection.has_value()) {
      return {};
    }

    Math::Vec3 intersectionPoint = intersection.value();
    Math::Vec3 translation = intersectionPoint - moveData.Origin;
    translation.x = 0.0F;
    translation.y = 0.0F;

    return translation;
  }
  case TransformAxis::XY: // Plane on to XY plane
  {
    Math::Plane plane(moveData.Origin, Math::Vec3(0.0F, 0.0F, 1.0F));
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(plane);
    if (!intersection.has_value()) {
      return {};
    }

    Math::Vec3 intersectionPoint = intersection.value();
    Math::Vec3 translation = intersectionPoint - moveData.Origin;
    translation.z = 0.0F;

    return translation;
  }
  case TransformAxis::XZ: // Plane on to XZ plane
  {
    Math::Plane plane(moveData.Origin, Math::Vec3(0.0F, 1.0F, 0.0F));
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(plane);
    if (!intersection.has_value()) {
      return {};
    }

    Math::Vec3 intersectionPoint = intersection.value();
    Math::Vec3 translation = intersectionPoint - moveData.Origin;
    translation.y = 0.0F;

    return translation;
  }
  case TransformAxis::YZ: // Plane on to YZ plane
  {
    Math::Plane plane(moveData.Origin, Math::Vec3(1.0F, 0.0F, 0.0F));
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(plane);
    if (!intersection.has_value()) {
      return {};
    }

    Math::Vec3 intersectionPoint = intersection.value();
    Math::Vec3 translation = intersectionPoint - moveData.Origin;
    translation.x = 0.0F;

    return translation;
  }
  case TransformAxis::XYZ: {
    Math::Vec3 originalPoint =
        moveData.OriginalRay.PointAt(moveData.OriginalDistance);
    Math::Vec3 currentPoint = moveData.OriginalRay.PointAt(moveData.Distance);
    Math::Vec3 translation = currentPoint - originalPoint;
    return translation;
  }
  }

  return {};
}

auto GizmoRotation(const MoveData &moveData, const Math::Ray &currentRay)
    -> Math::Quaternion {
  switch (moveData.Axis) {
  case TransformAxis::None:
  case TransformAxis::XYZ:
    return {};
  case TransformAxis::X:
  case TransformAxis::YZ: { // Rotate around X axis
    Math::Vec3 axis = Math::Vec3(1.0F, 0.0F, 0.0F);
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(
        Math::Plane(moveData.Origin, axis));
    if (!intersection.has_value()) {
      return {};
    }
    auto intersection2 =
        currentRay.IntersectPlane2Way(Math::Plane(moveData.Origin, axis));
    if (!intersection2.has_value()) {
      return {};
    }

    Math::Vec3 originalPoint = intersection.value() - moveData.Origin;
    Math::Vec3 currentPoint = intersection2.value() - moveData.Origin;
    Math::Vec2 original2D(originalPoint.y, originalPoint.z);
    Math::Vec2 current2D(currentPoint.y, currentPoint.z);
    float angle = std::atan2(current2D.y, current2D.x) -
                  std::atan2(original2D.y, original2D.x);
    Math::EulerAngle eulerAngle(angle, 0.0F, 0.0F);
    Math::Quaternion rotation = Math::Conversions::ToQuaternion(eulerAngle);

    return rotation;
  }
  case TransformAxis::Y:
  case TransformAxis::XZ: { // Rotate around Y axis
    Math::Vec3 axis = Math::Vec3(0.0F, 1.0F, 0.0F);
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(
        Math::Plane(moveData.Origin, axis));
    if (!intersection.has_value()) {
      return {};
    }
    auto intersection2 =
        currentRay.IntersectPlane2Way(Math::Plane(moveData.Origin, axis));
    if (!intersection2.has_value()) {
      return {};
    }

    Math::Vec3 originalPoint = intersection.value() - moveData.Origin;
    Math::Vec3 currentPoint = intersection2.value() - moveData.Origin;
    Math::Vec2 original2D(originalPoint.x, originalPoint.z);
    Math::Vec2 current2D(currentPoint.x, currentPoint.z);
    float angle = std::atan2(current2D.y, current2D.x) -
                  std::atan2(original2D.y, original2D.x);
    Math::EulerAngle eulerAngle(0.0F, angle, 0.0F);
    Math::Quaternion rotation = Math::Conversions::ToQuaternion(eulerAngle);

    return rotation;
  }
  case TransformAxis::Z:
  case TransformAxis::XY: { // Rotate around Z axis
    Math::Vec3 axis = Math::Vec3(0.0F, 0.0F, 1.0F);
    auto intersection = moveData.OriginalRay.IntersectPlane2Way(
        Math::Plane(moveData.Origin, axis));
    if (!intersection.has_value()) {
      return {};
    }
    auto intersection2 =
        currentRay.IntersectPlane2Way(Math::Plane(moveData.Origin, axis));
    if (!intersection2.has_value()) {
      return {};
    }

    Math::Vec3 originalPoint = intersection.value() - moveData.Origin;
    Math::Vec3 currentPoint = intersection2.value() - moveData.Origin;
    Math::Vec2 original2D(originalPoint.x, originalPoint.y);
    Math::Vec2 current2D(currentPoint.x, currentPoint.y);
    float angle = std::atan2(current2D.y, current2D.x) -
                  std::atan2(original2D.y, original2D.x);
    Math::EulerAngle eulerAngle(0.0F, 0.0F, angle);
    Math::Quaternion rotation = Math::Conversions::ToQuaternion(eulerAngle);

    return rotation;
  }
  }
}

auto GizmoScale(const MoveData &moveData) -> Math::Vec3 {
  Math::Vec2 mouseDelta =
      moveData.CurrentMousePosition - moveData.StartMousePosition;
  auto size =
      Window::GetDimensions(Graphics::GetCurrentGraphicsContext()->sdlWindow);
  mouseDelta.x /= float(size.x);
  mouseDelta.y /= float(size.y);

  float scaling = (mouseDelta.x + mouseDelta.y) * 10.0F; // NOLINT
  if (scaling < 0.0) {
    scaling = 1.0F / -scaling;
  } else {
    scaling = 1.0F + scaling;
  }

  switch (moveData.Axis) {
  case TransformAxis::None:
    return {};
  case TransformAxis::X:
    return {scaling, 1.0F, 1.0F};
  case TransformAxis::Y:
    return {1.0F, scaling, 1.0F};
  case TransformAxis::Z:
    return {1.0F, 1.0F, scaling};
  case TransformAxis::XY:
    return {scaling, scaling, 1.0F};
  case TransformAxis::XZ:
    return {scaling, 1.0F, scaling};
  case TransformAxis::YZ:
    return {1.0F, scaling, scaling};
  case TransformAxis::XYZ:
    return {scaling, scaling, scaling};
  }
}

void TransformGizmo(MoveData &moveData, const Math::Ray &currentRay) {
  switch (moveData.Mode) {
  default:
    return;
  case TransformMode::Translate: {
    Math::Vec3 translation = GizmoTranslation(moveData, currentRay);
    Math::Vec3 newTranslation = translation - moveData.CurrentTranslation;

    for (auto *transform : moveData.Transforms) {
      transform->ApplyTranslation(newTranslation);
    }

    moveData.CurrentTranslation = translation;

    break;
  }
  case TransformMode::Rotate: {
    Math::Quaternion rotation = GizmoRotation(moveData, currentRay);
    Math::Quaternion newRotation =
        rotation * moveData.CurrentRotation.Inverse();

    for (auto *transform : moveData.Transforms) {
      transform->ApplyRotation(newRotation);
    }

    moveData.CurrentRotation = rotation;

    break;
  }
  case TransformMode::Scale: {
    Math::Vec3 scale = GizmoScale(moveData);
    Math::Vec3 scaling = scale / moveData.CurrentScale;

    for (auto *transform : moveData.Transforms) {
      transform->ApplyScaling(scaling);
    }

    moveData.CurrentScale = scale;

    break;
  }
  }
}

auto MoveData::Update() -> void {
  if ((UpdatedTransformAxis || StartedTransforming) && !Transforms.empty()) {
    StartMousePosition = CurrentMousePosition;

    Origin = {};
    for (const auto *transform : Transforms) {
      Origin += transform->GetPosition();
    }

    StartedTransforming = false;
  }

  if (Transforms.empty()) {
    Axis = TransformAxis::None;
    Mode = TransformMode::None;
    StartedTransforming = false;
    UpdatedTransformAxis = false;

    return;
  }

  Math::Ray currentRay(
      Editor::EditorCamera.get<Transform>().GetPosition(),
      Editor::EditorCamera.get<CameraMatrices>().InverseProject_Rotation(
          Math::Vec3{CurrentMousePosition, 0.0F}));

  TransformGizmo(*this, currentRay);
}

auto MoveData::Apply() -> void {
  for (auto *transform : Transforms) {

    transform->ApplyTranslation(CurrentTranslation);
    transform->ApplyRotation(CurrentRotation);
    transform->ApplyScaling(CurrentScale);
  }

  CurrentTranslation = {};
  CurrentRotation = {};
  CurrentScale = {};
}

auto Editor::StartTranslating() -> void {
  if (Editor::MoveInfo.Mode == TransformMode::None) {
    Editor::MoveInfo.StartedTransforming = true;
  }

  Editor::MoveInfo.Mode = TransformMode::Translate;
}

auto Editor::StartRotating() -> void {
  if (Editor::MoveInfo.Mode == TransformMode::None) {
    Editor::MoveInfo.StartedTransforming = true;
  }

  Editor::MoveInfo.Mode = TransformMode::Rotate;
}

auto Editor::StartScaling() -> void {
  if (Editor::MoveInfo.Mode == TransformMode::None) {
    Editor::MoveInfo.StartedTransforming = true;
  }

  Editor::MoveInfo.Mode = TransformMode::Scale;
}

auto Editor::SetTransformAxis(TransformAxis axis) -> void {
  if (Editor::MoveInfo.Axis != axis) {
    Editor::MoveInfo.UpdatedTransformAxis = true;
  }

  Editor::MoveInfo.Axis = axis;
}

auto Editor::FinalizeTransform() -> void { Editor::MoveInfo.Apply(); }

inline auto DrawArrow(const Math::Vec3 &origin, const Math::Vec3 &direction,
                      float length, float headLength, float headWidth,
                      const Math::PackedColor &color, float thickness) -> void {
  auto &lineDrawer = Renderer::RendererInstance.GetLineDrawer();
  auto &primDrawer = Renderer::RendererInstance.GetPrimitiveDrawer();

  Math::Vec3 arrowTip = origin + direction * length;
  lineDrawer.OverlayLine(origin, arrowTip, color, thickness);

  const auto &tangent = direction.GetTangent();
  Math::Vec3 bitangent = direction.Cross(tangent).Normalize();

  Math::Vec3 headBase = arrowTip - direction * headLength;

  // NOLINTBEGIN
  const int segments = 16;
  for (int i = 0; i < segments; ++i) {
    float angle1 = (2.0F * M_PI * i) / segments;
    float angle2 = (2.0F * M_PI * (i + 1)) / segments;

    float c1 = std::cos(angle1) * headWidth;
    float s1 = std::sin(angle1) * headWidth;
    float c2 = std::cos(angle2) * headWidth;
    float s2 = std::sin(angle2) * headWidth;

    Math::Vec3 headPoint1 = headBase + tangent * c1 + bitangent * s1;
    Math::Vec3 headPoint2 = headBase + tangent * c2 + bitangent * s2;

    primDrawer.OverlayPrimitive(arrowTip, headPoint1, headPoint2, color);
  }

  // NOLINTEND
}

auto Editor::DrawGizmo() -> void {
  const auto &camera = EditorCamera.get<Camera>();
  const auto &matrices = EditorCamera.get<CameraMatrices>();

  auto scale =
      (MoveInfo.Origin - matrices.GetPosition()).Length() * 0.1F; // NOLINT
  auto &lineDrawer = Renderer::RendererInstance.GetLineDrawer();
  const auto thickness = 4.0F;
  auto originToCamera = (matrices.GetPosition() - MoveInfo.Origin).Normalize();

  auto arrowLength = scale;
  auto arrowHeadLength = scale * 0.2F; // NOLINT
  auto arrowHeadWidth = scale * 0.1F;  // NOLINT

  // X axis
  DrawArrow(MoveInfo.Origin, Math::Vec3{1.0F, 0.0F, 0.0F}, arrowLength,
            arrowHeadLength, arrowHeadWidth,
            Math::PackedColor{1.0F, 0.0F, 0.0F, 1.0F}, thickness);

  // Y axis
  DrawArrow(MoveInfo.Origin, Math::Vec3{0.0F, 1.0F, 0.0F}, arrowLength,
            arrowHeadLength, arrowHeadWidth,
            Math::PackedColor{0.0F, 1.0F, 0.0F, 1.0F}, thickness);

  // Z axis
  DrawArrow(MoveInfo.Origin, Math::Vec3{0.0F, 0.0F, 1.0F}, arrowLength,
            arrowHeadLength, arrowHeadWidth,
            Math::PackedColor{0.0F, 0.0F, 1.0F, 1.0F}, thickness);
}
} // namespace Engine