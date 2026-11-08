#include "wrap_math.hpp"
#include "Modules/Math/eulerAngle.hpp"
#include "Modules/Math/math.hpp"
#include "Modules/Math/matrix.hpp"
#include "Wrap/wrap.hpp"
#include <lua.hpp>

namespace Wrap::Math {
using namespace ::Math::Conversions;

using EulerAngle = ::Math::EulerAngle;
using Quaternion = ::Math::Quaternion;
using Matrix = ::Math::Matrix4x4;
using Scalar = ::Math::Scalar;

inline auto QuaternionToLua(lua_State *state, const Quaternion &quat) -> int {
  lua_pushnumber(state, quat.x);
  lua_pushnumber(state, quat.y);
  lua_pushnumber(state, quat.z);
  lua_pushnumber(state, quat.w);

  return 4;
}

inline auto EulerToLua(lua_State *state, const EulerAngle &euler) -> int {
  lua_pushnumber(state, euler.pitch);
  lua_pushnumber(state, euler.yaw);
  lua_pushnumber(state, euler.roll);

  return 3;
}

auto wrap_EulerToQuaternion(lua_State *state) -> int {
  auto pitch = luaL_checkscalar(state, 1);
  auto yaw = luaL_checkscalar(state, 2);
  auto roll = luaL_checkscalar(state, 3);

  auto quat = ToQuaternion(EulerAngle{pitch, yaw, roll});

  lua_pushnumber(state, quat.x);
  lua_pushnumber(state, quat.y);
  lua_pushnumber(state, quat.z);
  lua_pushnumber(state, quat.w);

  return 4;
}

auto wrap_EulerToMatrix(lua_State *state) -> int {
  auto pitch = luaL_checkscalar(state, 1);
  auto yaw = luaL_checkscalar(state, 2);
  auto roll = luaL_checkscalar(state, 3);

  auto mat = ToMatrix(EulerAngle{pitch, yaw, roll});

  return mat.ToLua(state);
}

auto wrap_QuaternionToEuler(lua_State *state) -> int {
  auto quat_x = luaL_checkscalar(state, 1);
  auto quat_y = luaL_checkscalar(state, 2);
  auto quat_z = luaL_checkscalar(state, 3);
  auto quat_w = luaL_checkscalar(state, 4);

  auto euler = ToEuler(Quaternion{quat_x, quat_y, quat_z, quat_w});

  return EulerToLua(state, euler);
}
auto wrap_QuaternionToMatrix(lua_State *state) -> int {
  auto quat_x = luaL_checkscalar(state, 1);
  auto quat_y = luaL_checkscalar(state, 2);
  auto quat_z = luaL_checkscalar(state, 3);
  auto quat_w = luaL_checkscalar(state, 4);

  auto mat = ToMatrix(Quaternion{quat_x, quat_y, quat_z, quat_w});

  return mat.ToLua(state);
}

auto wrap_MatrixToEuler(lua_State *state) -> int {
  // Check that we have a table with 16 numbers
  if (lua_objlen(state, 1) != 16) { // NOLINT
    return luaL_error(state, "Expected a table with 16 numbers for matrix");
  }

  auto euler = ToEuler(::Math::Matrix4x4::FromLua(state, 1));

  return EulerToLua(state, euler);
}
auto wrap_MatrixToQuaternion(lua_State *state) -> int {
  if (lua_objlen(state, 1) != 16) { // NOLINT
    return luaL_error(state, "Expected a table with 16 numbers for matrix");
  }

  auto quat = ToQuaternion(::Math::Matrix4x4::FromLua(state, 1));

  return QuaternionToLua(state, quat);
}

auto wrap_TranslationMatrix(lua_State *state) -> int {
  auto mat = Matrix::TranslationMatrix(luaL_checkscalar(state, 1), luaL_checkscalar(state, 2), luaL_checkscalar(state, 3));

  return mat.ToLua(state);
}

auto wrap_ScaleMatrix(lua_State *state) -> int {
  auto mat = Matrix::ScaleMatrix(luaL_checkscalar(state, 1), luaL_checkscalar(state, 2), luaL_checkscalar(state, 3));

  return mat.ToLua(state);
}

auto wrap_TransformMatrix(lua_State *state) -> int {
  // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  auto mat = Matrix::TransformationMatrix(luaL_checkscalar(state, 1), luaL_checkscalar(state, 2), luaL_checkscalar(state, 3), luaL_checkscalar(state, 4),
                                          luaL_checkscalar(state, 5), luaL_checkscalar(state, 6), luaL_checkscalar(state, 7), luaL_checkscalar(state, 8),
                                          luaL_checkscalar(state, 9), luaL_checkscalar(state, 10));
  // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

  return mat.ToLua(state);
}

auto wrap_Random(lua_State *state) -> int {
  if (lua_gettop(state) == 0) {
    auto result = ::Math::Random();
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 1) {
    auto max = static_cast<int>(luaL_checkinteger(state, 1));
    auto result = ::Math::Random(max);
    lua_pushinteger(state, result);
    return 1;
  }

  if (lua_gettop(state) == 2) {

    auto min = luaL_checkinteger(state, 1);
    auto max = luaL_checkinteger(state, 2);

    auto result = ::Math::Random(min, max);
    lua_pushinteger(state, result);
    return 1;
  }

  return luaL_error(state, "Invalid number of arguments to math.random");
}

auto wrap_Noise(lua_State *state) -> int {
  if (lua_gettop(state) == 1) {
    auto x_channel = luaL_checkscalar(state, 1);

    auto result = ::Math::Noise(x_channel, 0);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 2) {
    auto x_channel = luaL_checkscalar(state, 1);
    auto y_channel = luaL_checkscalar(state, 2);

    auto result = ::Math::Noise(x_channel, y_channel, 0, 0);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 3) {
    auto x_channel = luaL_checkscalar(state, 1);
    auto y_channel = luaL_checkscalar(state, 2);
    auto z_channel = luaL_checkscalar(state, 3);

    auto result = ::Math::Noise(x_channel, y_channel, z_channel, 0, 0, 0);
    lua_pushnumber(state, result);
    return 1;
  }

  return luaL_error(state, "Invalid number of arguments to math.noise");
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
auto wrap_NoiseWrapped(lua_State *state) -> int {
  if (lua_gettop(state) == 2) {
    auto x_channel = luaL_checkscalar(state, 1);
    auto x_wrap = static_cast<uint>(luaL_checkinteger(state, 2));

    auto result = ::Math::Noise(x_channel, x_wrap);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 4) {
    auto x_channel = luaL_checkscalar(state, 1);
    auto y_channel = luaL_checkscalar(state, 2);
    auto x_wrap = static_cast<uint>(luaL_checkinteger(state, 3));
    auto y_wrap = static_cast<uint>(luaL_checkinteger(state, 4));

    auto result = ::Math::Noise(x_channel, y_channel, x_wrap, y_wrap);
    lua_pushnumber(state, result);
    return 1;
  }

  if (lua_gettop(state) == 6) {
    auto x_channel = luaL_checkscalar(state, 1);
    auto y_channel = luaL_checkscalar(state, 2);
    auto z_channel = luaL_checkscalar(state, 3);
    auto x_wrap = static_cast<uint>(luaL_checkinteger(state, 4));
    auto y_wrap = static_cast<uint>(luaL_checkinteger(state, 5));
    auto z_wrap = static_cast<uint>(luaL_checkinteger(state, 6));

    auto result = ::Math::Noise(x_channel, y_channel, z_channel, x_wrap, y_wrap, z_wrap);
    lua_pushnumber(state, result);
    return 1;
  }

  return luaL_error(state, "Invalid number of arguments to math.noise");
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

} // namespace Wrap::Math
