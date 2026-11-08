#include "Modules/reflectBindings.hpp"
#include "Wrap/wrap.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lua.hpp>

#include "../SnapEngine/Wrap/wrap_engine.hpp"

auto RegisterAllLuaModules(lua_State *state) -> void {
  LuaWrap::RegisterModules(state);
  Engine::LuaWrap::RegisterModules(state);
}

auto main(int argCount, char **argValues) -> int {
  std::vector<std::string> args;
  args.reserve(argCount);

  std::string programName = argValues[0]; // NOLINT

  for (int i = 1; i < argCount; ++i) {
    args.emplace_back(argValues[i]); // NOLINT
  }

  bool verbose = false;

  // Allowed flags: -v for verbose output, -h for help
  for (const auto &arg : args) {
    if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: " << programName << " [options]\n";
      std::cout << "Options:\n";
      std::cout << "  -h, --help     Show this help message\n";
      std::cout << "  -v, --verbose  Enable verbose output\n";
      return 0;
    }

    if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      return 1;
    }
  }

  if (!verbose) {
    std::cout.setstate(std::ios_base::failbit);
  }

  auto *state = luaL_newstate();
  std::cout << "Generating Lua bindings...\n";
  RegisterAllLuaModules(state);

  // Log real path
  std::cout << "Current working directory: " << std::filesystem::current_path().string() << "\n";

  std::cout << "Emitting Lua bindings to 'generated/' directory...\n";
  std::filesystem::create_directories("generated");
  for (const auto &module : Bindings::LuaModules) {
    std::string filename = "generated/" + module.first + "_bindings.d.lua";
    std::cout << "Generating bindings for module: " << module.first << " -> " << filename << "\n";
    std::ofstream outFile(filename);

    if (!outFile) {
      std::cerr << "Failed to open file for module: " << module.first << "\n";
      continue;
    }

    Bindings::EmitLuaStruct(outFile, module.first, module.second);
    outFile.close();
  }

  std::cout << "Emitting Lua enums to 'generated/lua_enums.d.lua'...\n";
  std::ofstream enumOutFile("generated/lua_enums.d.lua");
  if (!enumOutFile) {
    std::cerr << "Failed to open file for Lua enums\n";
    return 1;
  }

  Bindings::EmitLuaEnums(enumOutFile, LuaWrap::RegisteredEnums);
  enumOutFile.close();

  std::cout << "Emitting Lua type aliases to 'generated/lua_aliases.d.lua'...\n";
  std::ofstream aliasOutFile("generated/lua_aliases.d.lua");
  if (!aliasOutFile) {
    std::cerr << "Failed to open file for Lua type aliases\n";
    return 1;
  }

  Bindings::EmitLuaAliases(aliasOutFile, Bindings::LuaTypeAliases);
  aliasOutFile.close();

  lua_close(state);
  std::cout << "Lua bindings generation completed.\n";
}
