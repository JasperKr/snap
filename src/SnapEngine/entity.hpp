#pragma once

#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include <flecs.h>
namespace Engine {

const static Type entityType = Type("Entity");

struct Entity : flecs::entity, Object {
  using flecs::entity::entity;

  static auto GetType() -> const Type * { return &entityType; }
  auto GetInstanceType() const -> const Type * override { return &entityType; }

  bool enabled; // TODO: Implement
};
} // namespace Engine