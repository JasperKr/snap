#include "wrap.hpp"
#include "Modules/console.hpp"
#include "Wrap/Modules/wrap_data.hpp"
#include "Wrap/Modules/wrap_math.hpp"
#include "Wrap/Modules/wrap_mouse.hpp"
#include "Wrap/Modules/wrap_thread.hpp"
#include "Wrap/Modules/wrap_window.hpp"
#include "Wrap/proxy.hpp"

#include <cstdint>
#include <iostream>
#include <lua.hpp>
#include <string>
#include <unordered_set>
#include <vector>

#include "Modules/object.hpp"
#include "Wrap/Graphics/wrap_graphics.hpp"
#include "Wrap/Modules/wrap_event.hpp"
#include "Wrap/Modules/wrap_filesystem.hpp"
#include "Wrap/Modules/wrap_keyboard.hpp"
#include "Wrap/Modules/wrap_timer.hpp"

// Enable if encountering C++ exceptions
// Requires the Enable RTTI and Enable Exceptions options to be enabled in CMakeLists.txt
#define DEBUG_CPP_EXCEPTION

namespace LuaWrap {

static auto wrap_gc(lua_State *state) -> int {
  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, 1));

  if (proxy != nullptr) {
    if (proxy->object == nullptr) {
      return 0;
    }

    proxy->object->release();
    proxy->object = nullptr;

    if (proxy->type == nullptr) { // Collecting module.
      return 0;
    }
  }
  return 0;
}

// NOLINTNEXTLINE
static auto wrap_tostring(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);

  if (proxy == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushfstring(state, "%s: %p", proxy->type->GetName().c_str(), static_cast<void *>(proxy->object));
  return 1;
}

static auto wrap_type(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);

  if (proxy == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushstring(state, proxy->type->GetName().c_str());
  return 1;
}

static auto wrap_typeof(lua_State *state) -> int {
  Proxy *proxy = ProxyFromLua(state, 1);

  if (proxy == nullptr) {
    lua_pushnil(state);
    return 1;
  }

  const auto *typeName = luaL_checkstring(state, 2);
  bool sameType = (proxy->type->GetName() == typeName);
  lua_pushboolean(state, sameType ? 1 : 0);
  return 1;
}

static auto wrap_eq(lua_State *state) -> int {
  Proxy *proxyA = ProxyFromLua(state, 1);
  Proxy *proxyB = ProxyFromLua(state, 2);

  if (proxyA == nullptr || proxyB == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  bool isEqual = (proxyA->object == proxyB->object);
  lua_pushboolean(state, isEqual ? 1 : 0);
  return 1;
}

static auto wrap_release(lua_State *state) -> int {
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, 1));

  if (proxy == nullptr) {
    lua_pushboolean(state, 0);
    return 1;
  }

  Object *object = proxy->object;

  if (object != nullptr) {
    object->release();
    proxy->object = nullptr;

    // load object storage table
    LoadStorageTable(state, "SnapObjectStorage"); // [storage]

    // NOLINTNEXTLINE
    lua_pushlightuserdata(state, (void *)object); // [storage, key]
    lua_pushnil(state);                           // [storage, key, nil]
    lua_settable(state, -3);                      // storage[key] = nil  [storage]

    lua_pop(state, 1); // []
  }

  // Success if object is now null
  lua_pushboolean(state, proxy->object == nullptr ? 1 : 0);

  return 1;
}

auto SetStackToTable(lua_State *state, const char *key) -> void {
  lua_getglobal(state, key);

  if (!lua_istable(state, -1)) { // Not found
    lua_pop(state, 1);           // Remove non-table value
    lua_newtable(state);
    lua_pushvalue(state, -1);
    lua_setglobal(state, key);
  }
}

auto LoadStorageTable(lua_State *state, const char *key) -> void { lua_getfield(state, LUA_REGISTRYINDEX, key); }

auto LoadOrCreateStorageTable(lua_State *state, const char *key) -> void {
  lua_getfield(state, LUA_REGISTRYINDEX, key); // [storage]

  if (!lua_istable(state, -1)) { // no table found
    lua_newtable(state);         // Create new table [nil, new table]
    lua_replace(state, -2);      // replace nil in registry with new table [storage]

    // Create metatable with weak values
    lua_newtable(state);               // [storage, mt]
    lua_pushstring(state, "v");        // [storage, mt, "v"]
    lua_setfield(state, -2, "__mode"); // mt.__mode = "v" [storage, mt]
    lua_setmetatable(state, -2);       // new table .mt = mt, popped mt [storage]

    lua_pushvalue(state, -1); // [storage, storage]
    lua_setfield(state, LUA_REGISTRYINDEX,
                 key); // registry[key] = storage [storage]
  }
}

using LuaFn = int (*)(lua_State *);

// Exceptions must be enabled for this to work!
#ifdef DEBUG_CPP_EXCEPTION
auto LuaTrampoline(lua_State *state) -> int {
  auto func = // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<LuaFn>(lua_touserdata(state, lua_upvalueindex(1)));

  try {
    return func(state);
  } catch (const std::exception &e) {
    return luaL_error(state, "C++ exception: %s", e.what());
  } catch (const char *str) {
    return luaL_error(state, "C++ exception: %s", str);
  } catch (int val) {
    return luaL_error(state, "C++ exception: int %d", val);
  } catch (...) {
    // Check if luaL_error was called, just return it normally.
    if (lua_type(state, -1) == LUA_TSTRING) {
      return lua_error(state);
    }

    return luaL_error(state, "Unknown C++ exception");
  }
}
#endif

static const Type moduleType("MODULE");

auto RegisterLuaModule(lua_State *state, const LuaModule &module) -> void {
  if (module.Functions.empty()) {
    std::cerr << "Module " << module.Name << " has no functions to register."
              << "\n";
    return;
  }

  SetStackToTable(state, "_modules"); // [_modules]

  // Create userdata for the module NOLINTNEXTLINE
  auto *proxy = (Proxy *)lua_newuserdata(state, sizeof(Proxy));
  proxy->type = &moduleType;
  proxy->object = nullptr;

  const auto *name = module.Name.c_str();

  luaL_newmetatable(state, name);     // Create metatable [_modules, mt]
  lua_pushvalue(state, -1);           // Duplicate metatable [_modules, mt, mt]
  lua_setfield(state, -2, "__index"); // mt.__index = mt [_modules, mt]
  lua_pushcfunction(state, wrap_gc);  // [_modules, mt, wrap__gc]
  lua_setfield(state, -2, "__gc");    // mt.__gc = wrap__gc [_modules, mt]

  lua_setmetatable(state, -2);   // [_modules, module] Set metatable for userdata
  lua_setfield(state, -2, name); // [_modules] _modules[name] = module
  lua_pop(state, 1);             // []

  SetStackToTable(state, "snap"); // [snap]

  lua_newtable(state); // [snap, module]

  // register Functions to snap.modulename.functionname
  if (!module.Functions.empty()) {
    for (const auto &func : module.Functions) {
      lua_pushcfunction(state, func.func); // [mt, func]
      lua_setfield(state, -2, func.name);  // [snap, module]
    }
  }

  lua_setfield(state, -2, name); // [snap]

  // register init functions
  if (!module.ChildrenInitFunctions.empty()) {
    for (const auto &func : module.ChildrenInitFunctions) {
      func(state);
    }
  }

  // done, remove module table from stack
  lua_pop(state, 1); // []
}

auto RegisterLuaType(lua_State *state, const Type *type, const std::vector<luaL_Reg> &functions) -> void {
  RegisterLuaType(state, LuaClass{.Name = type->GetName(), .Type = type, .Methods = functions, .Children = {}});
}

auto RegisterLuaType(lua_State *state, const LuaClass &luaClass) -> void {

  // Make sure permanent object storage table exists with weak values
  LoadOrCreateStorageTable(state, "SnapObjectStorage"); // [storage]

  lua_pop(state, 1); // Remove storage table from stack []

  if (luaClass.Type == nullptr) {
    PrintError("Lua type {} has null Type pointer.", luaClass.Name);
    return;
  }

  const auto *name = luaClass.Type->GetName().c_str();

  luaL_newmetatable(state, name);     // Create metatable [mt]
  lua_pushvalue(state, -1);           // Duplicate metatable [mt, mt]
  lua_setfield(state, -2, "__index"); // mt.__index = mt [mt]
  lua_pushcfunction(state, wrap_gc);  // [mt, wrap__gc]
  lua_setfield(state, -2, "__gc");    // mt.__gc = wrap__gc [mt]
  lua_pushcfunction(state, wrap_tostring);
  lua_setfield(state, -2, "__tostring"); // mt.__tostring = wrap__tostring [mt]
  lua_pushcfunction(state, wrap_type);
  lua_setfield(state, -2, "type"); // mt.type = wrap_type [mt]
  lua_pushcfunction(state, wrap_typeof);
  lua_setfield(state, -2, "typeof"); // mt.typeof = wrap_typeof [mt]
  lua_pushcfunction(state, wrap_eq);
  lua_setfield(state, -2, "__eq"); // mt.__eq = wrap_eq [mt]
  lua_pushcfunction(state, wrap_release);
  lua_setfield(state, -2, "release"); // mt.release = wrap_release [mt]

  static const std::unordered_set<std::string> ReservedNames = {
      "__gc", "__index", "__tostring", "type", "typeof", "__eq", "release",
  };

  for (const auto &func : luaClass.Methods) {
    if (func.name == nullptr) {
      PrintError("Cannot register Lua type {}: function with null name.", luaClass.Type->GetName());
      continue;
    }

    // Check for reserved names
    if (ReservedNames.contains(func.name)) {
      PrintError("Cannot register Lua type {}: function name '{}' is "
                 "reserved.",
                 luaClass.Type->GetName(), func.name);
      continue;
    }
#if !defined(NDEBUG) && defined(DEBUG_CPP_EXCEPTION) // If debug build
    // Wrap function in trampoline to catch exceptions
    lua_pushlightuserdata(state,              // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
                          (void *)func.func); // [mt, lightuserdata with function pointer]
    lua_pushcclosure(state, LuaTrampoline,
                     1); // [mt, cclosure that calls function pointer]
#else
    lua_pushcfunction(state, func.func); // [mt, func]
#endif
    lua_setfield(state, -2, func.name); // [mt]

#ifndef NDEBUG
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto firstChar = func.name[0];
    if (firstChar != '\0' && (std::isupper(firstChar) != 0)) {
      PrintWarning("Expected camelCase. Got: '{}' in class: '{}'.", func.name, luaClass.Type->GetName());
    }
#endif
  }

  for (const auto &component : luaClass.Components) {
    for (const auto &registration : component.Functions) {
      if (ReservedNames.contains(registration.name)) {
        PrintError("Cannot register Lua type {}: component name '{}' is "
                   "reserved.",
                   luaClass.Type->GetName(), registration.name);
        continue;
      }

      lua_pushstring(state, registration.name);
      lua_pushcfunction(state, registration.func);
      lua_settable(state, -3); // mt[componentName] = wrap_type [mt]
    }
  }

  lua_pop(state, 1); // []
}

auto SetupLuaType(lua_State *state, const Type *type, Object *object) -> void {

  if (object == nullptr) {
    lua_pushnil(state);
    return;
  }

  // NOLINTNEXTLINE
  auto *proxy = (Proxy *)lua_newuserdata(state, sizeof(Proxy)); // [userdata]
  proxy->type = type;
  proxy->object = object;

  if (type == nullptr) {
    PrintError("SetupLuaType: type is null");
    return;
  }

  object->retain();

  const auto *name = type->GetName().c_str();

  luaL_newmetatable(state, name); // Get metatable, [userdata, mt]

  lua_getfield(state, -1, "__gc");
  bool hasGC = (lua_isfunction(state, -1) != 0);
  lua_pop(state, 1);

  if (!hasGC) {
    PrintWarning("Lua type {} does not have a __gc method. Meaning stuff like "
                 ":type, etc... will not work either.",
                 name);
    // Add GC function
    lua_pushcfunction(state, wrap_gc); // [userdata, mt, wrap__gc]
    lua_setfield(state, -2, "__gc");   // mt.__gc = wrap__gc [userdata, mt]
  }

  lua_setmetatable(state, -2); // Set metatable for userdata, [userdata]
}

auto PushObject(lua_State *state, const Type *type, Object *object) -> void {
  if (object == nullptr) {
    lua_pushnil(state);
    return;
  }

  // fetch permanent object storage table
  LoadOrCreateStorageTable(state, "SnapObjectStorage"); // [storage]

  if (lua_isnoneornil(state, -1)) {
    // No storage table

    lua_pop(state, 1); // Remove nil [empty]

    lua_pushnil(state); // Push nil since we cannot store the object

    PrintError("PushObject: storage table missing, cannot store object.");
    return;
  }

  // Check if object already has a userdata
  lua_pushlightuserdata(state, (void *)object); // [storage, key]
  lua_gettable(state, -2);                      // [storage, value]

  if (lua_type(state, -1) != LUA_TLIGHTUSERDATA && lua_type(state, -1) != LUA_TUSERDATA) {

    // No existing userdata
    lua_pop(state, 1); // Remove nil [storage]

    // Create new userdata
    SetupLuaType(state, type, object); // [storage, userdata]

    // Store in storage table
    lua_pushlightuserdata(state, (void *)object); // [storage, userdata, key]
    lua_pushvalue(state, -2);                     // [storage, userdata, key, userdata]
    lua_settable(state, -4);                      // storage[key] = userdata [storage, userdata]
  }

  lua_remove(state, -2); // Remove storage table [userdata]
}

auto PushObject(lua_State *state, Object *object) -> void {
  if (object == nullptr) {
    lua_pushnil(state);
    return;
  }

  // fetch permanent object storage table
  LoadStorageTable(state, "SnapObjectStorage"); // [storage]

  if (lua_isnoneornil(state, -1)) {
    // No storage table

    lua_pop(state, 1); // Remove nil [empty]

    SetupLuaType(state, object->GetInstanceType(), object); // [userdata]
    return;
  }

  // Check if object already has a userdata
  lua_pushlightuserdata(state, (void *)object); // [storage, key] NOLINT
  lua_gettable(state, -2);                      // [storage, value]

  if (lua_type(state, -1) != LUA_TUSERDATA) {
    // No existing userdata
    lua_pop(state, 1); // Remove nil [storage]

    // Create new userdata
    SetupLuaType(state, object->GetInstanceType(),
                 object); // [storage, userdata]

    // Store in storage table
    lua_pushlightuserdata(state, (void *)object); // [storage, userdata, key]
    lua_pushvalue(state, -2);                     // [storage, userdata, key, userdata]
    lua_settable(state, -4);                      // storage[key] = userdata [storage, userdata]
  }

  lua_remove(state, -2); // Remove storage table [userdata]
}

// NOLINTNEXTLINE
static const std::vector<luaL_Reg> SnapModules = {
    {"graphics", Wrap::Graphics::luaopen_graphics}, {"event", Wrap::Event::luaopen_event},
    {"timer", Wrap::Timer::luaopen_timer},          {"data", Wrap::Data::luaopen_data},
    {"thread", Wrap::Threading::luaopen_threading}, {"filesystem", Wrap::Filesystem::luaopen_filesystem},
    {"keyboard", Wrap::Keyboard::luaopen_keyboard}, {"mouse", Wrap::Mouse::luaopen_mouse},
    {"window", Wrap::Window::luaopen_window},       {"math", Wrap::Math::luaopen_engine_math},
};

auto RegisterModules(lua_State *state) -> void {
  lua_getglobal(state, "snap"); // [snap or nil]
  if (!lua_istable(state, -1)) {
    lua_newtable(state);          // [nil, table]
    lua_setglobal(state, "snap"); // set global 'snap'
  }
  // At this point, 'snap' table is on top of stack
  lua_pop(state, 1); // empty stack

  for (const auto &module : SnapModules) {
    module.func(state);
  }
}

} // namespace LuaWrap

// No namespace.

auto luaL_checkscalar(lua_State *state, int index) -> Math::Scalar { return static_cast<Math::Scalar>(luaL_checknumber(state, index)); }

auto luaL_optscalar(lua_State *state, int index, Math::Scalar defaultValue) -> Math::Scalar {
  if (lua_isnoneornil(state, index)) {
    return defaultValue;
  }
  return static_cast<Math::Scalar>(luaL_checknumber(state, index));
}