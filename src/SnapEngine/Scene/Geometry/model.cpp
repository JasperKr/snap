#include "model.hpp"
#include "Modules/Math/quaternion.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Scene/Geometry/shape.hpp"
#include "Scene/scene.hpp"
#include "Scene/transform.hpp"
#include "Scene/userdata.hpp"
#include "Wrap/wrap.hpp"
#include "Wrap/wrap_engine.hpp"
#include <lua.hpp>

namespace Engine {
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

// world:createModel("model", { x, y, z }, { rotX, rotY, rotZ, rotW }, { scaleX, scaleY, scaleZ }, { shape1, shape2 }, material)
auto LuaModel::Create(lua_State *state) -> int {
  auto scene = LUA_CK_NULL(::LuaWrap::ObjectFromLua<Scene>(state, 1));

  const char *name = luaL_checkstring(state, 2);
  auto entity = scene->world.entity(name);

  entity.add<Model>();
  entity.add<Userdata>();

  Math::Vec3 position{0.0F, 0.0F, 0.0F};
  // Position table
  if (lua_gettop(state) >= 3) {
    luaL_checktype(state, 3, LUA_TTABLE);
    lua_rawgeti(state, 3, 1);
    position.x = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 3, 2);
    position.y = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 3, 3);
    position.z = static_cast<float>(luaL_checknumber(state, -1));
    lua_pop(state, 3);
  }

  // Rotation table
  Math::Quaternion rotation{0.0F, 0.0F, 0.0F, 1.0F};
  if (lua_gettop(state) >= 4) {
    luaL_checktype(state, 4, LUA_TTABLE);
    lua_rawgeti(state, 4, 1);
    rotation.x = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 4, 2);
    rotation.y = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 4, 3);
    rotation.z = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 4, 4);
    rotation.w = static_cast<float>(luaL_checknumber(state, -1));
    lua_pop(state, 4);
  }

  // Scale table
  Math::Vec3 scale{1.0F, 1.0F, 1.0F};
  if (lua_gettop(state) >= 5) {
    luaL_checktype(state, 5, LUA_TTABLE);
    lua_rawgeti(state, 5, 1);
    scale.x = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 5, 2);
    scale.y = static_cast<float>(luaL_checknumber(state, -1));
    lua_rawgeti(state, 5, 3);
    scale.z = static_cast<float>(luaL_checknumber(state, -1));
    lua_pop(state, 3);
  }

  entity.set<Transform>(Transform(position, rotation, scale));

  if (lua_gettop(state) >= 6) {
    luaL_checktype(state, 6, LUA_TTABLE);
    lua_pushnil(state);
    while (lua_next(state, 6) != 0) {
      auto *shapeEntity = ::LuaWrap::EntityFromLua(state, -1);
      if (shapeEntity != nullptr) {
        shapeEntity->child_of(entity);
      } else {
        return luaL_error(state, "Expected a Shape object in the shapes table");
      }
      lua_pop(state, 1);
    }
  }

  if (lua_gettop(state) >= 7) {
    auto *materialEntity = ::LuaWrap::EntityFromLua(state, 7);
    if (materialEntity != nullptr) {
      materialEntity->child_of(entity);
    }
  }

  auto model = Ref<LuaModel>::Make(entity);
  ::LuaWrap::PushObject(state, LuaModel::GetType(), model.get());

  return 1;
}

auto LuaModel::GetShapes(lua_State *state) -> int {
  auto *entity = LUA_CK_NULL(::LuaWrap::EntityFromLua(state, 1));

  if (lua_gettop(state) == 1) {
    lua_newtable(state);
  } else {
    luaL_checktype(state, 2, LUA_TTABLE);
  }

  int index = 1;
  entity->children<Shape>([&](flecs::entity entity) -> void {
    lua_pushinteger(state, index++);
    auto shapeObject = LuaShape::FromEntity(entity);
    ::LuaWrap::PushObject(state, LuaShape::GetType(), shapeObject.get());
    lua_settable(state, -3);
  });

  return 1;
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
} // namespace Engine