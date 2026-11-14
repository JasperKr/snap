#pragma once

#include "Modules/error.hpp"

namespace Engine::Physics {

auto Initialize() -> Error;
auto DeInitialize() -> void;

} // namespace Engine::Physics