#pragma once

#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/type.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_set>
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
  } else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, const char *> ||
                       std::is_same_v<T, std::basic_string<char>>) {
    return BindingLuaType::String;
  } else if constexpr (std::is_same_v<T, Math::Vec2> ||
                       std::is_same_v<T, Math::Uvec2> ||
                       std::is_same_v<T, Math::Ivec2>) {
    return BindingLuaType::Vec2;
  } else if constexpr (std::is_same_v<T, Math::Vec3> ||
                       std::is_same_v<T, Math::Uvec3> ||
                       std::is_same_v<T, Math::Ivec3>) {
    return BindingLuaType::Vec3;
  } else if constexpr (std::is_same_v<T, Math::Vec4> ||
                       std::is_same_v<T, Math::Uvec4> ||
                       std::is_same_v<T, Math::Ivec4>) {
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

inline auto LuaTypeName(BindingLuaType bindingType) -> std::string {
  static std::vector<std::string> types;
  types.clear();
  auto bindings = static_cast<uint32_t>(bindingType);

  for (auto bit : Utils::BitMaskRange(bindings)) {
    switch (static_cast<BindingLuaType>(bit)) {
    case BindingLuaType::Integer:
      types.emplace_back("integer");
      break;
    case BindingLuaType::Number:
      types.emplace_back("number");
      break;
    case BindingLuaType::Boolean:
      types.emplace_back("boolean");
      break;
    case BindingLuaType::String:
      types.emplace_back("string");
      break;
    case BindingLuaType::Table:
      types.emplace_back("table");
      break;
    case BindingLuaType::Userdata:
      types.emplace_back("snap.Data"); // handled separately
      break;
    case BindingLuaType::Vec2:
      types.emplace_back("Vec2");
      break;
    case BindingLuaType::Vec3:
      types.emplace_back("Vec3");
      break;
    case BindingLuaType::Vec4:
      types.emplace_back("Vec4");
      break;
    case BindingLuaType::Quaternion:
      types.emplace_back("Quaternion");
      break;
    case BindingLuaType::Matrix4x4:
      types.emplace_back("Matrix4x4");
      break;
    default:
      types.emplace_back("any");
      break;
    }
  }

  if (types.empty()) {
    return "any";
  }
  if (types.size() == 1) {
    return types.at(0);
  } // Multiple types, return as union
  static std::string result;
  result.clear();
  for (size_t i = 0; i < types.size(); ++i) {
    if (i > 0) {
      result += " | ";
    }
    result += types[i];
  }
  return result;
}

inline auto LowercaseFirstChar(const std::string &str) -> std::string {
  if (str.empty()) {
    return str;
  }
  std::string result = str;
  result[0] = static_cast<char>(std::tolower(result[0]));
  return result;
}

inline auto GetLuaTypes(const std::vector<TypeInfo> &methods)
    -> std::vector<std::pair<std::string, std::string>> {
  std::vector<std::pair<std::string, std::string>> types;

  for (const auto &typeinfo : methods) {
    const auto &camelCaseName = LowercaseFirstChar(typeinfo.name);

    if (typeinfo.isEnum) {
      types.emplace_back(std::string("snap.") + typeinfo.enumHelper->name,
                         camelCaseName);
      continue;
    }

    if (typeinfo.luaType == BindingLuaType::Userdata &&
        (typeinfo.type != nullptr)) {
      types.emplace_back(std::string("snap.") + typeinfo.type->GetName(),
                         camelCaseName);
      continue;
    }

    const auto &base = LuaTypeName(typeinfo.luaType);
    if (typeinfo.isVector) {
      types.emplace_back(std::string(base) + "[]", camelCaseName);
    } else if (typeinfo.luaType == BindingLuaType::Vec2) {
      types.emplace_back("number", camelCaseName + "_x");
      types.emplace_back("number", camelCaseName + "_y");
    } else if (typeinfo.luaType == BindingLuaType::Vec3) {
      types.emplace_back("number", camelCaseName + "_x");
      types.emplace_back("number", camelCaseName + "_y");
      types.emplace_back("number", camelCaseName + "_z");
    } else if (typeinfo.luaType == BindingLuaType::Vec4 ||
               typeinfo.luaType == BindingLuaType::Quaternion) {
      types.emplace_back("number", camelCaseName + "_x");
      types.emplace_back("number", camelCaseName + "_y");
      types.emplace_back("number", camelCaseName + "_z");
      types.emplace_back("number", camelCaseName + "_w");
    } else if (typeinfo.luaType == BindingLuaType::Matrix4x4) {
      types.emplace_back("number[16]", camelCaseName);
    } else {
      types.emplace_back(base, camelCaseName);
    }
  }

  return types;
}

inline auto Replace(const std::string &src, char from,
                    const std::string &toReplace) -> std::string {
  std::string result;
  size_t start = 0;
  size_t pos = 0;
  while ((pos = src.find(from, start)) != std::string::npos) {
    result += src.substr(start, pos - start) + toReplace;
    start = pos + 1;
  }
  result += src.substr(start);
  return result;
}

inline void EmitLuaStruct(std::ostream &out, const std::string &classname,
                          const std::vector<MethodInfo> &members) {

  out << "---@meta\n";
  out << "-- This file is auto-generated. Do not edit directly.\n";
  out << "-- See reflectBindings.hpp or the lua_stub_gen target.\n\n";
  out << "error(\"Do not require this file.\")\n\n";

  out << "---@class snap." << classname << " : snap.Data\n";
  out << classname << " = {}\n\n";

  for (const auto &member : members) {
    if (!member.description.empty()) {
      const auto &replaced = Replace(member.description, '\n', "\n--- ");
      out << "--- " << replaced << "\n";
    }

    const auto &parameters = GetLuaTypes(member.parameters);
    auto fieldCount = parameters.size();

    for (const auto &type : parameters) {
      out << "---@param ";

      const auto &typeName = type.first;
      const auto &paramName = type.second;

      out << (paramName.empty() ? "param" : paramName) << " "
          << (typeName.empty() ? "any" : typeName) << "\n";
    }

    out << "---@return ";
    if (member.returnType.has_value()) {
      const auto &returnTypes = GetLuaTypes({member.returnType.value()});
      for (size_t index = 0; index < returnTypes.size(); ++index) {
        if (index > 0) {
          out << "---@return ";
        }

        const auto &typeName = returnTypes[index].first;
        const auto &paramName = returnTypes[index].second;

        out << (typeName.empty() ? "any" : typeName) << " "
            << (paramName.empty() ? "Unknown" : paramName) << "\n";
      }

      if (returnTypes.empty()) {
        out << "any\n";
      }
    } else {
      out << "nil\n";
    }

    out << "function " << classname << ":" << member.name << "(";

    for (size_t index = 0; index < fieldCount; ++index) {
      if (index > 0) {
        out << ", ";
      }

      out << parameters[index].second;
    }

    out << ") end\n\n";
  }
}

inline void EmitLuaEnums(std::ostream &out,
                         const std::vector<LuaWrap::LuaEnumBase> &enums) {
  out << "---@meta\n";
  out << "-- This file is auto-generated. Do not edit directly.\n";
  out << "-- See reflectBindings.hpp or the lua_stub_gen target.\n\n";
  out << "error(\"Do not require this file.\")\n\n";

  std::unordered_set<std::string> found;

  for (const auto &luaEnum : enums) {
    if (found.contains(luaEnum.name)) {
      continue;
    }
    found.emplace(luaEnum.name);

    out << "---@alias snap." << luaEnum.name << " ";
    bool first = true;
    for (const auto &option : luaEnum.options) {
      if (!first) {
        out << " | ";
      }

      out << "\"" << option << "\"";
      first = false;
    }
    out << "\n\n";
  }
}

inline auto EmitLuaAliases(std::ostream &out, const std::vector<Alias> &aliases)
    -> void {
  out << "---@meta\n";
  out << "-- This file is auto-generated. Do not edit directly.\n";
  out << "-- See reflectBindings.hpp or the lua_stub_gen target.\n\n";
  out << "error(\"Do not require this file.\")\n\n";

  for (const auto &alias : aliases) {
    out << "---@alias snap." << alias.name << " ";
    bool first = true;
    for (const auto &type : alias.types) {
      if (!first) {
        out << " | ";
      }

      if (type.isEnum) {
        out << "snap." << type.enumHelper->name;
      } else if (type.luaType == BindingLuaType::Userdata &&
                 (type.type != nullptr)) {
        out << "snap." << type.type->GetName();
      } else {
        out << LuaTypeName(type.luaType);
      }
      first = false;
    }
    out << "\n\n";
  }
}

} // namespace Bindings