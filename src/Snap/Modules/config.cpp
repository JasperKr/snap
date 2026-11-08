#include "config.hpp"
#include "Graphics/deviceSettings.hpp"
#include "Graphics/shader.hpp"
#include "Modules/console.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/Modules/wrap_window.hpp"
#include "Wrap/wrap.hpp"

#include <lua.hpp>
#include <string>

namespace Config {

// NOLINTNEXTLINE
ApplicationConfig globalConfig;

inline auto SetIdentity(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for Identity");
  }
  globalConfig.Identity = lua_tostring(state, 1);
  return 0;
}

inline auto StringToExtensionRequirement(const char *str) -> Result<Graphics::ExtensionRequirement> {
  if (std::string(str) == "disabled") {
    return Graphics::ExtensionRequirement::Disabled;
  }
  if (std::string(str) == "optional") {
    return Graphics::ExtensionRequirement::Optional;
  }
  if (std::string(str) == "required") {
    return Graphics::ExtensionRequirement::Required;
  }

  return Error::Unexpectedf("Invalid extension requirement '{}'", str);
}

const LuaWrap::LuaEnum<Graphics::ExtensionRequirement> ExtensionRequirementEnum("ExtensionRequirement",
                                                                                {
                                                                                    {"disabled", Graphics::ExtensionRequirement::Disabled},
                                                                                    {"optional", Graphics::ExtensionRequirement::Optional},
                                                                                    {"required", Graphics::ExtensionRequirement::Required},
                                                                                });

inline auto SetGraphicsSettings(lua_State *state) -> int {
  lua_getfield(state, 1, "hardwareRaytracing");
  if (!lua_isnoneornil(state, -1)) {
    globalConfig.deviceSettings.hardwareRaytracing = LUA_CK_RES(ExtensionRequirementEnum.FromLua(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "inlineRaytracing");
  if (!lua_isnoneornil(state, -1)) {
    globalConfig.deviceSettings.inlineRaytracing = LUA_CK_RES(ExtensionRequirementEnum.FromLua(state, -1));
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "requiredExtensions");
  if (!lua_isnoneornil(state, -1)) {
    if (!lua_istable(state, -1)) {
      return luaL_error(state, "requiredExtensions must be a table");
    }

    lua_pushnil(state);
    while (lua_next(state, -2) != 0) {
      if (lua_type(state, -2) != LUA_TSTRING || lua_type(state, -1) != LUA_TSTRING) {
        return luaL_error(state, "requiredExtensions must be a table "
                                 "with string keys and values");
      }
      const char *extName = lua_tostring(state, -2);
      const char *extReq = lua_tostring(state, -1);
      Graphics::ExtensionRequirement extRequirement{};

      auto extRequirementResult = StringToExtensionRequirement(extReq);
      if (Error::IsError(extRequirementResult)) {
        return luaL_error(state, "%s", extRequirementResult.error().message.c_str());
      }

      extRequirement = extRequirementResult.value();
      globalConfig.deviceSettings.requiredExtensions.emplace_back(Graphics::Extension{
          .name = extName,
          // Instance-level extensions are not supported by lua configuration.
          .type = Graphics::ExtensionType::Device,
          .requirement = extRequirement,
      });
    }
  }
  lua_pop(state, 1);

  return 0;
}

inline auto LuaSetLogLevel(lua_State *state) -> int {
  if (lua_isstring(state, 1) == 0) {
    return luaL_error(state, "Expected string for LogLevel");
  }
  const char *levelStr = lua_tostring(state, 1);
  if (levelStr == nullptr) {
    return luaL_error(state, "Invalid string for LogLevel");
  }

  std::string level(levelStr);
  if (level == "debug") {
    SetLogLevel(LogLevel::Debug);
  } else if (level == "info") {
    SetLogLevel(LogLevel::Info);
  } else if (level == "warning") {
    SetLogLevel(LogLevel::Warning);
  } else if (level == "error") {
    SetLogLevel(LogLevel::Error);
  } else if (level == "fatal") {
    SetLogLevel(LogLevel::Fatal);
  } else {
    return luaL_error(state, "Unknown LogLevel: %s", levelStr);
  }

  PrintInfo("LogLevel set to {}", levelStr);
  return 0;
}

inline auto SetShaderIncludePaths(lua_State *state) -> int {

  auto count = lua_objlen(state, 1);
  for (int i = 1; i <= count; ++i) {
    lua_rawgeti(state, 1, i);
    const auto *str = luaL_checkstring(state, -1);
    Graphics::ShaderSearchPaths.emplace_back(strdup(str));
    lua_pop(state, 1);
  }

  return 0;
}

inline auto SetFunctions(lua_State *state) -> void {
  lua_pushcfunction(state, SetIdentity);
  lua_setfield(state, -2, "_setIdentity");

  lua_pushcfunction(state, LuaSetLogLevel);
  lua_setfield(state, -2, "_setLogLevel");

  lua_pushcfunction(state, Wrap::Window::wrap_SetInitialSettings);
  lua_setfield(state, -2, "_setSettings");

  lua_pushcfunction(state, SetGraphicsSettings);
  lua_setfield(state, -2, "_setGraphicsSettings");

  lua_pushcfunction(state, SetShaderIncludePaths);
  lua_setfield(state, -2, "_setShaderIncludePaths");
}

inline auto RemoveFunctions(lua_State *state) -> void {
  LuaWrap::SetStackToTable(state, "snap"); // [snap]

  // clang-format off
  lua_pushstring(state, "_setIdentity"); lua_pushnil(state); lua_rawset(state, -3);
  lua_pushstring(state, "_setLogLevel"); lua_pushnil(state); lua_rawset(state, -3);
  lua_pushstring(state, "_setSettings"); lua_pushnil(state); lua_rawset(state, -3);
  lua_pushstring(state, "_setGraphicsSettings"); lua_pushnil(state); lua_rawset(state, -3);
  lua_pushstring(state, "_setShaderIncludePaths"); lua_pushnil(state); lua_rawset(state, -3);
  // clang-format on

  lua_pop(state, 1); // pop snap table
}

auto Configure(lua_State *state, const std::string &sourceDirectory) -> Result<ApplicationConfig> {

  LuaWrap::SetStackToTable(state, "snap"); // [snap]

  SetFunctions(state); // [snap with set functions]
  lua_pop(state, 1);   // Pop snap table

  const auto luaScript = R"lua(
    -- User configuration script
    package.path = package.path .. ";)lua" +
                         sourceDirectory + R"lua(/?.lua"

    local config = {
      filesystem = {
        identity = "MyGame",
      },
      window = {},
      graphics = {
        hardwareRaytracing = "disabled",
        inlineRaytracing = "disabled",
      },
      loglevel = ""
    }

    local success, err = pcall(require, "configuration")

    if not success then
      print("No configuration.lua found or error in file: " .. err)
    end

    if (type(snap.config) == "function") then
      -- Call user-defined configuration function, not a pcall, this needs to run without errors
      snap.config(config)
    end

    snap._setIdentity(config.filesystem.identity)
    snap._setSettings(config.window)
    snap._setGraphicsSettings(config.graphics or {})
    snap._setShaderIncludePaths(config.graphics and config.graphics.shaderIncludePaths or {})
    if config.loglevel ~= "" then snap._setLogLevel(config.loglevel) end
  )lua";

  if (luaL_dostring(state, luaScript.c_str()) != LUA_OK) {
    const char *errorMsg = lua_tostring(state, -1);
    lua_pop(state, 1); // Pop error message
    return Error::Unexpected(std::string("Lua error: ") + ((errorMsg != nullptr) ? errorMsg : "Unknown error"));
  }

  // Stack is clean here

  RemoveFunctions(state);

  return globalConfig;
}

} // namespace Config
