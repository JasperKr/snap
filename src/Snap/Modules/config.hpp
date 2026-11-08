#pragma once

#include "Graphics/deviceSettings.hpp"
#include <string>

extern "C" {
#include <lua.hpp>
}

#include "Modules/error.hpp"

namespace Config {

struct ApplicationConfig {
  std::string Identity = "snap"; // Filesystem identity

  Graphics::DeviceSettings deviceSettings;
};

// NOLINTNEXTLINE
extern ApplicationConfig globalConfig;

auto Configure(lua_State *state, const std::string &sourceDirectory) -> Result<ApplicationConfig>;

} // namespace Config