#pragma once

#include "Modules/Math/mathTypes.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/type.hpp"
#include "Wrap/lua_data.hpp"
#include "lua.hpp"
#include <Modules/object.hpp>
#include <flecs.h>
#include <variant>
#include <vector>

namespace LuaWrap {

struct LuaModule {
  // Name of the module
  std::string Name;

  std::vector<luaL_Reg> Functions;
  std::vector<lua_CFunction> ChildrenInitFunctions;
};

struct LuaComponent {
  std::vector<luaL_Reg> Functions;
};

struct LuaClass {
  std::string Name;
  Type const *Type;

  std::vector<luaL_Reg> Methods;
  std::vector<LuaComponent> Components; // For composition

  std::vector<LuaClass> Children;
};

auto RegisterLuaModule(lua_State *state, const LuaModule &module) -> void;
auto RegisterModules(lua_State *state) -> void;

// Sets the stack to a table stored in the registry under the given key
inline auto SetStackToRegistry(lua_State *state, const char *key) -> void {
  lua_getfield(state, LUA_REGISTRYINDEX, key);

  if (!lua_istable(state, -1)) { // Not found
    lua_pop(state, 1);           // Remove non-table value
    lua_newtable(state);
    lua_pushvalue(state, -1);
    lua_setfield(state, LUA_REGISTRYINDEX, key);
  }
}

auto SetStackToTable(lua_State *state, const char *key) -> void;
auto PushObject(lua_State *state, const Type *type, Object *object) -> void;
auto RegisterLuaType(lua_State *state, const Type *type, const std::vector<luaL_Reg> &functions) -> void;
auto RegisterLuaType(lua_State *state, const LuaClass &luaClass) -> void;
auto SetupLuaType(lua_State *state, const Type *type, Object *object) -> void;
auto LoadStorageTable(lua_State *state, const char *key) -> void;
auto LoadOrCreateStorageTable(lua_State *state, const char *key) -> void;
auto LogStack(lua_State *state) -> void;

// Test if T is a valid Lua userdata C++ object usable with a proxy.
template <typename T>
concept LuaObject = std::is_base_of_v<Object, std::remove_cvref_t<T>>;

template <typename T>
  requires LuaObject<T>
inline auto ObjectFromLua(lua_State *state, int index) -> Ref<T> {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return Ref<T>(nullptr);
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    PrintWarning("FromLuaObject: proxy is null at index {}", index);
    return Ref<T>(nullptr);
  }

  if (proxy->object == nullptr || proxy->type == nullptr) {
    PrintWarning("FromLuaObject: proxy invalid at index {}", index);
    return Ref<T>(nullptr);
  }

  // Do not compare type addresses, they may differ since the types are
  // Defined inline, so addresses are different for each translation unit
  if (proxy->type->GetName() != T::GetType()->GetName()) {
    auto proxyTypeName = proxy->type->GetName();
    auto expectedTypeName = T::GetType()->GetName();

    PrintWarning("FromLuaObject: type mismatch at index {}, expected: {} got: {}", index, expectedTypeName, proxyTypeName);
    return Ref<T>(nullptr);
  }

  auto *obj = static_cast<T *>(proxy->object);
  return Ref<T>(obj);
}

template <typename T>
  requires LuaObject<T>
inline auto ResultFromLua(lua_State *state, int index) -> Result<T *> {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return Error::Unexpected("Expected userdata at index " + std::to_string(index));
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    return Error::Unexpected("Proxy is null at index " + std::to_string(index));
  }

  if (proxy->object == nullptr || proxy->type == nullptr) {
    return Error::Unexpected("Proxy is invalid at index " + std::to_string(index));
  }

  // Do not compare type addresses, they may differ since the types are
  // Defined inline, so addresses are different for each translation unit
  if (proxy->type->GetName() != T::GetType()->GetName()) {
    auto proxyTypeName = proxy->type->GetName();
    auto expectedTypeName = T::GetType()->GetName();

    return Error::Unexpected("Type mismatch at index " + std::to_string(index) + ", expected: " + expectedTypeName + " got: " + proxyTypeName);
  }

  auto *obj = static_cast<T *>(proxy->object);
  return obj;
}

inline auto PushPointer(lua_State *state, void *pointer) -> void {
  // // NOLINTNEXTLINE
  // auto **userdata =
  //     static_cast<void **>(lua_newuserdata(state, sizeof(void *)));
  // *userdata = pointer;
  lua_pushlightuserdata(state, pointer);
}

inline auto LuaType(lua_State *state, int index) -> Type const * {
  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    PrintWarning("LuaType: proxy is null at index {}", index);
    return nullptr;
  }

  if (proxy->type == nullptr) {
    PrintWarning("LuaType: proxy type is null at index {}", index);
    return nullptr;
  }

  if (proxy->object == nullptr) {
    PrintWarning("LuaType: proxy object is null at index {}", index);
    return nullptr;
  }

  return proxy->type;
}

template <typename T> inline auto IsType(lua_State *state, int index) -> bool {
  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr || proxy->object == nullptr) {
    return false;
  }

  return proxy->type->GetName() == T::GetType()->GetName();
}

inline auto SerializeVarargs(lua_State *state, int startIndex) -> Result<std::vector<Data::LuaType>> {

  int numArgs = lua_gettop(state) - startIndex + 1;

  std::vector<Data::LuaType> launchArguments;
  launchArguments.reserve(static_cast<size_t>(numArgs));

  for (int i = 0; i < numArgs; ++i) {
    if (lua_isnoneornil(state, startIndex + i)) {
      launchArguments.emplace_back(std::monostate{});
      continue;
    }

    auto args = CHECK_RES(Data::FromStack(state, startIndex + i));
    launchArguments.emplace_back(args);
  }

  return launchArguments;
}

inline auto PushVarargs(lua_State *state, const std::vector<Data::LuaType> &launchArguments, int count) -> Error {

  for (int i = 0; i < count; ++i) {
    const auto &arg = launchArguments[static_cast<size_t>(i)];

    auto pushResult = Data::ToStack(state, arg);
    if (Error::IsError(pushResult)) {
      return pushResult;
    }
  }

  return Error::Success();
}

} // namespace LuaWrap

// No namespace.
auto luaL_checkscalar(lua_State *state, int index) -> Math::Scalar;
auto luaL_optscalar(lua_State *state, int index, Math::Scalar defaultValue) -> Math::Scalar;
auto luaL_optboolean(lua_State *state, int index, bool defaultValue) -> bool;

template <typename T> struct ObjectTypeNameBase {
  using type = std::remove_cvref_t<T>;
};

template <typename T> struct ObjectTypeNameBase<Ref<T>> {
  using type = T;
};

template <typename T> using ObjectTypeNameBaseT = typename ObjectTypeNameBase<std::remove_cvref_t<T>>::type;

template <typename T> constexpr auto ObjectTypeName() -> std::string_view {
  using Base = ObjectTypeNameBaseT<T>;

  if constexpr (std::is_base_of_v<Object, Base>) {
    return Base::GetType()->GetName();
  } else {
    return "pointer";
  }
}

// NOLINTBEGIN

#define LUA_CK_NULL(expr)                                                                                                                                      \
  ({                                                                                                                                                           \
    auto &&_result = (expr);                                                                                                                                   \
    [[unlikely]]                                                                                                                                               \
    if (_result == nullptr) {                                                                                                                                  \
      auto typeName = ObjectTypeName<decltype(_result)>();                                                                                                     \
      return luaL_error(state, "Null %s at " #expr, typeName.data());                                                                                          \
    }                                                                                                                                                          \
    std::move(_result);                                                                                                                                        \
  })

#define LUA_CK_NULL_MSG(expr, msg)                                                                                                                             \
  ({                                                                                                                                                           \
    auto &&_result = (expr);                                                                                                                                   \
    [[unlikely]]                                                                                                                                               \
    if (_result == nullptr) {                                                                                                                                  \
      return luaL_error(state, "%s", (msg));                                                                                                                   \
    }                                                                                                                                                          \
    std::move(_result);                                                                                                                                        \
  })

#define LUA_CK_ERR(expr)                                                                                                                                       \
  {                                                                                                                                                            \
    auto _error = std::move(expr);                                                                                                                             \
    [[unlikely]]                                                                                                                                               \
    if (Error::IsError(_error)) {                                                                                                                              \
      return luaL_error(state, "%s", _error.ToString().c_str());                                                                                               \
    }                                                                                                                                                          \
  }

#define LUA_CK_RES(expr)                                                                                                                                       \
  ({                                                                                                                                                           \
    auto &&_result = (expr);                                                                                                                                   \
    [[unlikely]]                                                                                                                                               \
    if (Error::IsError(_result)) {                                                                                                                             \
      return luaL_error(state, "%s", _result.error().ToString().c_str());                                                                                      \
    }                                                                                                                                                          \
    std::move(_result.value());                                                                                                                                \
  })

#define LUA_ASSERT(expr)                                                                                                                                       \
  {                                                                                                                                                            \
    [[unlikely]]                                                                                                                                               \
    if (!(expr)) {                                                                                                                                             \
      return luaL_error(state, "Assertion failed: " #expr);                                                                                                    \
    }                                                                                                                                                          \
  }

#define LUA_ASSERT_MSG(expr, msg)                                                                                                                              \
  {                                                                                                                                                            \
    [[unlikely]]                                                                                                                                               \
    if (!(expr)) {                                                                                                                                             \
      return luaL_error(state, "%s", (msg));                                                                                                                   \
    }                                                                                                                                                          \
  }

/*
template <typename... Args> // NOLINTNEXTLINE args forwarding
  static auto Unexpectedf(std::string_view format, Args &&...args) {
    return tl::unexpected<Error>(Create(std::vformat(format, std::make_format_args(args...)), -1, 1));
  }
*/

#define LUA_ASSERT_MSGF(expr, msg, ...)                                                                                                                        \
  {                                                                                                                                                            \
    [[unlikely]]                                                                                                                                               \
    if (!(expr)) {                                                                                                                                             \
      std::string formattedMsg = std::format(msg, __VA_ARGS__);                                                                                                \
      return luaL_error(state, "%s", formattedMsg.c_str());                                                                                                    \
    }                                                                                                                                                          \
  }

// NOLINTEND
