#pragma once

#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Scene/transform.hpp"
#include "eventBase.hpp"
#include <flecs.h>

namespace Engine::Editor {

struct TransformEvent : public EventBase {
  Math::Vec3 PositionDelta{};
  Math::Quaternion RotationDelta;
  Math::Vec3 ScaleDelta{};

  TransformEvent(const Math::Vec3 &positionDelta, const Math::Quaternion &rotationDelta, const Math::Vec3 &scaleDelta)
      : PositionDelta(positionDelta), RotationDelta(rotationDelta), ScaleDelta(scaleDelta) {}

  auto Apply(flecs::entity entity) -> void override {
    auto *transform = entity.try_get_mut<Transform>();
    if (transform != nullptr) {
      transform->ApplyTranslation(PositionDelta);
      transform->ApplyRotation(RotationDelta);
      transform->ApplyScaling(ScaleDelta);
    }
  }

  auto Revert(flecs::entity entity) -> void override {
    auto *transform = entity.try_get_mut<Transform>();
    if (transform != nullptr) {
      transform->ApplyTranslation(-PositionDelta);
      transform->ApplyRotation(RotationDelta.Inverse());
      transform->ApplyScaling(Math::Vec3(1.0F / ScaleDelta.x, 1.0F / ScaleDelta.y, 1.0F / ScaleDelta.z));
    }
  }
};

} // namespace Engine::Editor