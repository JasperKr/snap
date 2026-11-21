#pragma once

#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/type.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
namespace Bindings {

// NOLINTNEXTLINE, uint16_t would become int16_t?? Idk.. which will warn on Bit-op
enum class BindingLuaType : uint32_t {
  // Default types
  Integer = 1U << 0U,
  Number = 1U << 1U,
  Boolean = 1U << 2U,
  String = 1U << 3U,
  Table = 1U << 4U,
  Userdata = 1U << 5U,

  // Special types with custom handling
  Vec2 = 1U << 6U,
  Vec3 = 1U << 7U,
  Vec4 = 1U << 8U,
  Quaternion = 1U << 9U,
  Matrix4x4 = 1U << 10U,

  // Types that can be safely used across threads
  ThreadSafe = Integer | Number | Boolean | String | Userdata,
};

template <typename T> constexpr auto GetBindingLuaType() {
  if constexpr (std::is_integral_v<T>) {
    return BindingLuaType::Integer;
  } else if constexpr (std::is_floating_point_v<T>) {
    return BindingLuaType::Number;
  } else if constexpr (std::is_same_v<T, bool>) {
    return BindingLuaType::Boolean;
  } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const char *> || std::is_same_v<T, std::basic_string<char>>) {
    return BindingLuaType::String;
  } else if constexpr (std::is_same_v<T, Math::Vec2> || std::is_same_v<T, Math::Uvec2> || std::is_same_v<T, Math::Ivec2>) {
    return BindingLuaType::Vec2;
  } else if constexpr (std::is_same_v<T, Math::Vec3> || std::is_same_v<T, Math::Uvec3> || std::is_same_v<T, Math::Ivec3>) {
    return BindingLuaType::Vec3;
  } else if constexpr (std::is_same_v<T, Math::Vec4> || std::is_same_v<T, Math::Uvec4> || std::is_same_v<T, Math::Ivec4>) {
    return BindingLuaType::Vec4;
  } else if constexpr (std::is_same_v<T, Math::Quaternion>) {
    return BindingLuaType::Quaternion;
  } else if constexpr (std::is_same_v<T, Math::Matrix4x4>) {
    return BindingLuaType::Matrix4x4;
  } else {
    return BindingLuaType::Userdata; // Default to userdata for complex types
  }
}

struct TypeInfo {
  std::string name;

  Type const *type; // For Ref<T>
  BindingLuaType luaType;
  bool isEnum = false;
  bool isVector = false;
  const LuaWrap::LuaEnumBase *enumHelper;
};

struct MethodInfo {
  std::string name;
  std::string description;

  std::vector<TypeInfo> parameters;
  std::optional<TypeInfo> returnType;
};

struct Alias {
  std::string name;
  std::vector<TypeInfo> types;
  Type newLuaType;
};

// NOLINTNEXTLINE
extern std::vector<std::pair<std::string, std::vector<MethodInfo>>> LuaModules;

// NOLINTNEXTLINE
inline std::vector<Alias> LuaTypeAliases;

auto LuaTypeName(BindingLuaType bindingType) -> std::string;

auto LowercaseFirstChar(const std::string &str) -> std::string;

auto GetLuaTypes(const std::vector<TypeInfo> &methods) -> std::vector<std::pair<std::string, std::string>>;

auto Replace(const std::string &src, char from, const std::string &toReplace) -> std::string;

void EmitLuaStruct(std::ostream &out, const std::string &classname, const std::vector<MethodInfo> &members);

void EmitLuaEnums(std::ostream &out, const std::vector<LuaWrap::LuaEnumBase> &enums);

auto EmitLuaAliases(std::ostream &out, const std::vector<Alias> &aliases) -> void;

} // namespace Bindings