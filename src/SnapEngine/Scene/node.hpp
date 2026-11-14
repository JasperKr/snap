#pragma once

#include <flecs.h>
namespace Engine {

struct Node {
  auto DrawGUI(flecs::entity entity) -> void;
};

} // namespace Engine