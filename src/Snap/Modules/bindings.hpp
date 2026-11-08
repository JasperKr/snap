// Lua bindings generator

#pragma once

#include "Modules/Math/matrix.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/Helpers/lua_vector.hpp"
#include "Wrap/wrap.hpp"
#include "lua.hpp"
#include "reflectBindings.hpp"
#include <cassert>
#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Bindings {

template <typename> struct is_ref : std::false_type {};
template <typename T> struct is_ref<Ref<T>> : std::true_type {
  using element_type = T;
};

// Check if R is a Ref<O> Where O inherits from Object.
template <typename R>
concept RefOfObject = is_ref<R>::value && std::is_base_of_v<Object, typename is_ref<R>::element_type>;

// Pointer or reference types are returned as pointers, value types are returned as values.
template <typename T>
using LuaGetReturn = std::conditional_t<std::is_arithmetic_v<std::remove_cvref_t<T>> || std::is_same_v<std::remove_cvref_t<T>, std::string> || RefOfObject<T> ||
                                            std::is_trivially_constructible<T>(),
                                        std::remove_cvref_t<T>,  // value types
                                        std::remove_cvref_t<T> * // userdata types
                                        >;

template <typename> struct MemberTraits;

template <typename T, typename M> struct MemberTraits<M T::*> {
  using Object = T;
  using Type = M;
};

template <typename T> struct LuaType {
  // Returns either a value (for arithmetic and string types) or a pointer (for userdata types)
  template <typename M> static auto GetValue(lua_State *state, int stackIndex) -> Result<LuaGetReturn<M>> {
    if constexpr (std::is_floating_point_v<M>) {
      return static_cast<M>(luaL_checknumber(state, stackIndex));
    } else if constexpr (std::is_same_v<M, bool>) {
      return lua_toboolean(state, stackIndex) != 0;
    } else if constexpr (std::is_integral_v<M>) {
      return static_cast<M>(luaL_checkinteger(state, stackIndex));
    } else if constexpr (std::is_same_v<M, std::string>) {
      size_t len = 0;
      const char *str = luaL_checklstring(state, stackIndex, &len);
      return std::string(str, len);
    } else if constexpr (std::is_same_v<M, Math::Vec2> || std::is_same_v<M, Math::Vec3> || std::is_same_v<M, Math::Vec4> ||
                         std::is_same_v<M, Math::Quaternion> || std::is_same_v<M, Math::Matrix4x4>) {
      return GetMathValue<M>(state, stackIndex);
    } else if constexpr (LuaWrap::LuaObject<M>) {
      return LuaWrap::ResultFromLua<M>(state, stackIndex);
    } else {
      return Error::Unexpected("Unsupported type for Lua bindings");
    }

    return Error::Unexpected("Unsupported type for Lua bindings");
  };

  template <typename M> static auto PushValue(lua_State *state, const M &value) -> int {
    if constexpr (std::is_floating_point_v<M>) {
      lua_pushnumber(state, static_cast<lua_Number>(value));
    } else if constexpr (std::is_same_v<M, bool>) {
      lua_pushboolean(state, value);
    } else if constexpr (std::is_integral_v<M>) {
      lua_pushinteger(state, static_cast<lua_Integer>(value));
    } else if constexpr (std::is_same_v<M, std::string>) {
      lua_pushlstring(state, value.c_str(), value.size());
    } else if constexpr (std::is_same_v<M, Math::Vec2> || std::is_same_v<M, Math::Vec3> || std::is_same_v<M, Math::Vec4> ||
                         std::is_same_v<M, Math::Quaternion> || std::is_same_v<M, Math::Matrix4x4>) {
      return PushMathValue(state, value);
    } else if constexpr (LuaWrap::LuaObject<M>) {
      LuaWrap::PushObject(state, M::GetType(), const_cast<M *>(&value));
    } else if (RefOfObject<M>) {
      using O = typename M::element_type;
      LuaWrap::PushObject(state, O::GetType(), value.get());
    }

    return 1;
  };

  template <typename M>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  static auto GetMathValue(lua_State *state, int stackIndex) -> Result<M> {
    if constexpr (std::is_same_v<M, Math::Vec2>) {
      if (lua_gettop(state) - stackIndex + 1 < 2) {
        return Error::Unexpected("Expected at least 2 values for Vec2");
      }
      return M{static_cast<float>(luaL_checknumber(state, stackIndex)), static_cast<float>(luaL_checknumber(state, stackIndex + 1))};
    } else if constexpr (std::is_same_v<M, Math::Uvec2> || std::is_same_v<M, Math::Ivec2>) {
      if (lua_gettop(state) - stackIndex + 1 < 2) {
        return Error::Unexpected("Expected at least 2 values for Vec2");
      }
      return M{static_cast<uint32_t>(luaL_checkinteger(state, stackIndex)), static_cast<uint32_t>(luaL_checkinteger(state, stackIndex + 1))};
    } else if constexpr (std::is_same_v<M, Math::Vec3>) {
      if (lua_gettop(state) - stackIndex + 1 < 3) {
        return Error::Unexpected("Expected at least 3 values for Vec3");
      }
      return M{static_cast<float>(luaL_checknumber(state, stackIndex)), static_cast<float>(luaL_checknumber(state, stackIndex + 1)),
               static_cast<float>(luaL_checknumber(state, stackIndex + 2))};
    } else if constexpr (std::is_same_v<M, Math::Uvec3> || std::is_same_v<M, Math::Ivec3>) {
      if (lua_gettop(state) - stackIndex + 1 < 3) {
        return Error::Unexpected("Expected at least 3 values for Vec3");
      }
      return M{static_cast<uint32_t>(luaL_checkinteger(state, stackIndex)), static_cast<uint32_t>(luaL_checkinteger(state, stackIndex + 1)),
               static_cast<uint32_t>(luaL_checkinteger(state, stackIndex + 2))};
    } else if constexpr (std::is_same_v<M, Math::Vec4> || std::is_same_v<M, Math::Quaternion>) {
      if (lua_gettop(state) - stackIndex + 1 < 4) {
        std::string typeName = std::is_same_v<M, Math::Vec4> ? "Vec4" : "Quaternion";
        return Error::Unexpectedf("Expected at least 4 values for {}", typeName);
      }
      return M{static_cast<float>(luaL_checknumber(state, stackIndex)), static_cast<float>(luaL_checknumber(state, stackIndex + 1)),
               static_cast<float>(luaL_checknumber(state, stackIndex + 2)), static_cast<float>(luaL_checknumber(state, stackIndex + 3))};
    } else if constexpr (std::is_same_v<M, Math::Uvec4> || std::is_same_v<M, Math::Ivec4>) {
      if (lua_gettop(state) - stackIndex + 1 < 4) {
        return Error::Unexpected("Expected at least 4 values for Vec4");
      }
      return M{static_cast<uint32_t>(luaL_checkinteger(state, stackIndex)), static_cast<uint32_t>(luaL_checkinteger(state, stackIndex + 1)),
               static_cast<uint32_t>(luaL_checkinteger(state, stackIndex + 2)), static_cast<uint32_t>(luaL_checkinteger(state, stackIndex + 3))};
    } else if constexpr (std::is_same_v<M, Math::Matrix4x4>) {
      if (lua_gettop(state) - stackIndex + 1 < Math::Matrix4x4::Size) {
        return Error::Unexpected("Expected at least 16 values for Mat4");
      }
      M mat;
      for (int i = 0; i < Math::Matrix4x4::Size; ++i) {
        mat.data[i] = static_cast<float>(luaL_checknumber(state, stackIndex + i));
      }
      return mat;
    } else {
      return GetValue<M>(state, stackIndex);
    }
  };

  template <typename M> static auto PushMathValue(lua_State *state, const M &value) -> int {
    if constexpr (std::is_same_v<M, Math::Vec2>) {
      lua_pushnumber(state, value.x);
      lua_pushnumber(state, value.y);
      return 2;
    } else if constexpr (std::is_same_v<M, Math::Uvec2> || std::is_same_v<M, Math::Ivec2>) {
      lua_pushinteger(state, value.x);
      lua_pushinteger(state, value.y);
      return 2;
    } else if constexpr (std::is_same_v<M, Math::Vec3>) {
      lua_pushnumber(state, value.x);
      lua_pushnumber(state, value.y);
      lua_pushnumber(state, value.z);
      return 3;
    } else if constexpr (std::is_same_v<M, Math::Uvec3> || std::is_same_v<M, Math::Ivec3>) {
      lua_pushinteger(state, value.x);
      lua_pushinteger(state, value.y);
      lua_pushinteger(state, value.z);
      return 3;
    } else if constexpr (std::is_same_v<M, Math::Vec4> || std::is_same_v<M, Math::Quaternion>) {
      lua_pushnumber(state, value.x);
      lua_pushnumber(state, value.y);
      lua_pushnumber(state, value.z);
      lua_pushnumber(state, value.w);
      return 4;
    } else if constexpr (std::is_same_v<M, Math::Uvec4> || std::is_same_v<M, Math::Ivec4>) {
      lua_pushinteger(state, value.x);
      lua_pushinteger(state, value.y);
      lua_pushinteger(state, value.z);
      lua_pushinteger(state, value.w);
      return 4;
    } else if constexpr (std::is_same_v<M, Math::Matrix4x4>) {
      for (size_t i = 0; i < Math::Matrix4x4::Size; ++i) {
        lua_pushnumber(state, value.data[i]);
      }
      return static_cast<int>(Math::Matrix4x4::Size);
    } else {
      PushValue(state, value);
      return 1;
    }
  };

  template <auto Member> static auto GenBindings_Get() -> lua_CFunction {
    using MemberPtr = decltype(Member); // e.g., int Test::*
    using Traits = MemberTraits<MemberPtr>;

    return [](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<T>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      // remove object from stack, so we don't return it to Lua if PushValue fails
      lua_pop(state, 1);

      return PushValue(state, static_cast<typename Traits::Object *>(*obj)->*Member);
    };
  }

  template <auto Member> static auto GenBindings_Set() -> lua_CFunction {
    using MemberPtr = decltype(Member); // e.g., int Test::*
    using Traits = MemberTraits<MemberPtr>;
    using M = typename Traits::Type;

    // God help you if it crashes here

    return [](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<T>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      // Assign dereferenced pointer for userdata types, direct value for value types
      if constexpr (std::is_pointer_v<LuaGetReturn<M>>) {
        if constexpr (std::is_copy_assignable_v<M> || std::is_move_assignable_v<M>) {
          auto value = GetValue<M>(state, 2);
          if (Error::IsError(value)) {
            return luaL_error(state, "%s", value.error().message.c_str());
          }

          static_cast<typename Traits::Object *>(*obj)->*Member = *value.value();
        } else {
          return luaL_error(state, "Type is not assignable");
        }
      } else {
        // If M is a RefOfObject, we need to assign the inner object, not the Ref itself
        if constexpr (RefOfObject<M>) {
          using O = typename M::element_type;

          auto value = LuaWrap::ResultFromLua<O>(state, 2);
          if (Error::IsError(value)) {
            return luaL_error(state, "%s", value.error().message.c_str());
          }

          static_cast<typename Traits::Object *>(*obj)->*Member = Ref<O>(value.value());
          return 0;
        }

        auto value = GetValue<M>(state, 2);
        if (Error::IsError(value)) {
          return luaL_error(state, "%s", value.error().message.c_str());
        }

        static_cast<typename Traits::Object *>(*obj)->*Member = value.value();
      }

      return 0;
    };
  };
};

[[nodiscard]] inline auto DefineLuaTypeAlias(const std::string &aliasName, const std::vector<TypeInfo> &types) -> Type & {
  LuaTypeAliases.emplace_back(Alias{.name = aliasName, .types = types, .newLuaType = Type(aliasName)});
  return LuaTypeAliases.back().newLuaType;
}

// Base class of LuaBoundStruct used for documentation purposes.
// You may use this if you have an entire class defined manually, and want to auto-generate documentation
struct LuaDocumentingStruct {
  std::string name;
  std::vector<MethodInfo> methodInfos;

  void DocumentCustomMethod(const MethodInfo &info) { methodInfos.emplace_back(info); }

  void DocumentCustomMethod(const std::string &name, // NOLINT
                            const std::string &description, const std::vector<TypeInfo> &parameters = {}, const std::optional<TypeInfo> &returnType = {}) {
    methodInfos.emplace_back(name, description, parameters, returnType);
  }

  void Register() { Bindings::LuaModules.emplace_back(name, methodInfos); }
  static auto CreateType(const std::string &name, BindingLuaType luaType) -> TypeInfo {
    return TypeInfo{
        .name = name,
        .type = nullptr,
        .luaType = luaType,
        .isEnum = false,
        .isVector = false,
    };
  }
  static auto CreateVectorType(const std::string &name, BindingLuaType luaType) -> TypeInfo {
    return TypeInfo{
        .name = name,
        .type = nullptr,
        .luaType = luaType,
        .isEnum = false,
        .isVector = true,
    };
  }
};

template <typename T>
  requires LuaWrap::LuaObject<T>
struct LuaBoundStruct : LuaDocumentingStruct {
  explicit LuaBoundStruct<T>(std::string structName) : LuaDocumentingStruct(std::move(structName)) {}

  std::vector<lua_CFunction> methods;
  std::vector<std::string> names;
  LuaType<T> type{};

  void AddMethod(const lua_CFunction &method) { methods.emplace_back(method); }

  // Usage: binding.RegisterMember<&MyStruct::myField>("MyField");
  template <auto Member>
  void RegisterMember(const std::string &name, // NOLINT
                      const std::string &description = "") {
    using Traits = MemberTraits<decltype(Member)>; // e.g., MemberTraits<int Test::*>
    using M = typename Traits::Type;               // e.g., int
    using Struct = typename Traits::Object;        // e.g., Test

    names.emplace_back(std::string("get") + name);
    AddMethod(type.template GenBindings_Get<Member>());
    methodInfos.emplace_back(MethodInfo{
        .name = std::string("get") + name,
        .description = description,
        .parameters = {},
        .returnType =
            TypeInfo{
                .name = name,
                .type = nullptr,
                .luaType = GetBindingLuaType<M>(),
                .isEnum = false,
                .isVector = false,
            },
    });
    if constexpr (GetBindingLuaType<M>() == BindingLuaType::Userdata) {
      methodInfos.back().returnType->type = M::GetType();
    }

    names.emplace_back(std::string("set") + name);
    AddMethod(type.template GenBindings_Set<Member>());

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("set") + name,
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = name,
                    .type = nullptr,
                    .luaType = GetBindingLuaType<M>(),
                    .isEnum = false,
                    .isVector = false,
                },
            },
    });

    if constexpr (GetBindingLuaType<M>() == BindingLuaType::Userdata) {
      methodInfos.back().parameters[0].type = M::GetType();
    }
  };

  template <auto Member>
  void RegisterStandardVectorMember(const std::string &name, // NOLINT
                                    const std::string &description = "") {
    // Similar to RegisterMember but for std::vector members, generates get, add, at, remove methods.

    using Traits = MemberTraits<decltype(Member)>; // e.g., MemberTraits<std::vector<int> Test::*>
    using V = typename Traits::Type;               // e.g., std::vector<int>
    static_assert(std::is_same_v<V, std::vector<typename V::value_type>>, "Member must be a std::vector");
    using ElementType = typename V::value_type;

    // get entire vector method
    names.emplace_back(std::string("get") + name + "List");
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      LuaWrap::PushVector(state, (*obj)->*Member,
                          [](lua_State *state, const ElementType &element) -> auto { LuaType<ElementType>::PushValue(state, element); });
      return 1;
    });

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("get") + name + "List",
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = name,
                    .type = Traits::Object::GetType(),
                    .luaType = GetBindingLuaType<typename Traits::Object>(),
                    .isEnum = false,
                    .isVector = false,
                },
            },
        .returnType =
            TypeInfo{
                .name = name,
                .type = nullptr,
                .luaType = GetBindingLuaType<ElementType>(),
                .isEnum = false,
                .isVector = false,
            },
    });
    if (methodInfos.back().returnType->luaType == BindingLuaType::Userdata) {
      methodInfos.back().returnType->type = Traits::Type::value_type::GetType();
    }

    // add element method
    names.emplace_back(std::string("add") + name);
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      auto elementValue = LuaType<ElementType>::template GetValue<ElementType>(state, 2);
      if (Error::IsError(elementValue)) {
        return luaL_error(state, "%s", elementValue.error().message.c_str());
      }

      ((*obj)->*Member).emplace_back(elementValue.value());
      return 0;
    });
    methodInfos.emplace_back(MethodInfo{
        .name = std::string("add") + name,
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = name,
                    .type = nullptr,
                    .luaType = GetBindingLuaType<ElementType>(),
                    .isEnum = false,
                    .isVector = false,
                },
            },
    });

    if (methodInfos.back().parameters[0].luaType == BindingLuaType::Userdata) {
      methodInfos.back().parameters[0].type = Traits::Type::value_type::element_type::GetType();
    }

    // at index method
    names.emplace_back(std::string("get") + name + "At");
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      int index = static_cast<int>(luaL_checkinteger(state, 2)) - 1;
      auto &vec = (*obj)->*Member;
      if (index < 0 || index >= static_cast<int>(vec.size())) {
        return luaL_error(state, "Index out of bounds");
      }

      LuaType<ElementType>::PushValue(state, vec[index]);
      return 1;
    });

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("get") + name + "At",
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = "Index",
                    .type = nullptr,
                    .luaType = BindingLuaType::Integer,
                    .isEnum = false,
                    .isVector = false,
                },
            },
        .returnType =
            TypeInfo{
                .name = name,
                .type = nullptr,
                .luaType = GetBindingLuaType<ElementType>(),
                .isEnum = false,
                .isVector = false,
            },
    });
    if (methodInfos.back().returnType->luaType == BindingLuaType::Userdata) {
      methodInfos.back().returnType->type = Traits::Type::value_type::element_type::GetType();
    }

    // remove at index method
    names.emplace_back(std::string("remove") + name + "At");
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      int index = static_cast<int>(luaL_checkinteger(state, 2)) - 1;
      auto &vec = (*obj)->*Member;
      if (index < 0 || index >= static_cast<int>(vec.size())) {
        return luaL_error(state, "Index out of bounds");
      }

      vec.erase(vec.begin() + index);
      return 0;
    });

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("remove") + name + "At",
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = "Index",
                    .type = nullptr,
                    .luaType = BindingLuaType::Integer,
                    .isEnum = false,
                    .isVector = false,
                },
            },
    });

    // Assert that ElementType is equality comparable for the remove by value method
    static_assert(std::equality_comparable<ElementType>, "ElementType must be equality comparable for remove method");

    // remove element method (by value, removes first occurrence)
    names.emplace_back(std::string("remove") + name);
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      auto elementValue = LuaType<ElementType>::template GetValue<ElementType>(state, 2);
      if (Error::IsError(elementValue)) {
        return luaL_error(state, "%s", elementValue.error().message.c_str());
      }

      auto &vec = (*obj)->*Member;
      auto iter = std::find(vec.begin(), vec.end(), elementValue.value());
      if (iter == vec.end()) {
        return luaL_error(state, "Element not found in vector");
      }

      vec.erase(iter);
      return 0;
    });

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("remove") + name,
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = name,
                    .type = nullptr,
                    .luaType = GetBindingLuaType<ElementType>(),
                    .isEnum = false,
                    .isVector = false,
                },
            },
    });

    if (methodInfos.back().parameters[0].luaType == BindingLuaType::Userdata) {
      methodInfos.back().parameters[0].type = Traits::Type::value_type::element_type::GetType();
    }

    // getItemCount method
    names.emplace_back(std::string("get") + name + "Count");
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      auto &vec = (*obj)->*Member;
      lua_pushinteger(state, static_cast<lua_Integer>(vec.size()));
      return 1;
    });

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("get") + name + "Count",
        .description = description,
        .parameters = {},
        .returnType =
            TypeInfo{
                .name = "Count",
                .type = nullptr,
                .luaType = BindingLuaType::Integer,
                .isEnum = false,
                .isVector = false,
            },
    });
  };

  template <auto Member, const LuaWrap::LuaEnum<typename MemberTraits<decltype(Member)>::Type> &EnumHelper>
  auto RegisterEnum(const std::string &name, // NOLINT
                    const std::string &description = "") -> void {
    using Traits = MemberTraits<decltype(Member)>;
    using M = typename Traits::Type;

    names.emplace_back(std::string("set") + name);
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      auto result = EnumHelper.FromLua(state, 2);
      if (Error::IsError(result)) {
        return luaL_error(state, "%s", result.error().message.c_str());
      }

      (*obj)->*Member = static_cast<M>(result.value());
      return 0;
    });
    methodInfos.emplace_back(MethodInfo{
        .name = std::string("set") + name,
        .description = description,
        .parameters =
            {
                TypeInfo{
                    .name = EnumHelper.name,
                    .type = nullptr,
                    .luaType = BindingLuaType::String,
                    .isEnum = true,
                    .isVector = false,
                    .enumHelper = &EnumHelper,
                },
            },
    });

    names.emplace_back(std::string("get") + name);
    AddMethod([](lua_State *state) -> int {
      auto obj = LuaWrap::ResultFromLua<typename Traits::Object>(state, 1);
      if (Error::IsError(obj)) {
        return luaL_error(state, "%s", obj.error().message.c_str());
      }

      auto value = (*obj)->*Member;
      auto result = EnumHelper.ToLua(state, value);
      if (Error::IsError(result)) {
        return luaL_error(state, "%s", result.message.c_str());
      }

      return 1;
    });

    methodInfos.emplace_back(MethodInfo{
        .name = std::string("get") + name,
        .description = description,
        .parameters = {},
        .returnType =
            TypeInfo{
                .name = EnumHelper.name,
                .type = nullptr,
                .luaType = BindingLuaType::String,
                .isEnum = true,
                .isVector = false,
                .enumHelper = &EnumHelper,
            },
    });
  };

  void Register(lua_State *state, const std::vector<std::pair<std::string, lua_CFunction>> &additionalMethods = {}) {
    assert(!name.empty() && "Struct name must be set before registering");

    std::vector<luaL_Reg> luaMethods;
    luaMethods.reserve(methods.size() + additionalMethods.size() + 1); // +1 for the null terminator
    for (size_t i = 0; i < methods.size(); ++i) {
      luaMethods.push_back({names[i].c_str(), methods[i]});
    }
    for (const auto &[methodName, methodFunc] : additionalMethods) {
      luaMethods.push_back({methodName.c_str(), methodFunc});
    }

    LuaDocumentingStruct::Register();

    LuaWrap::RegisterLuaType(state, T::GetType(), luaMethods);
  };
};

} // namespace Bindings