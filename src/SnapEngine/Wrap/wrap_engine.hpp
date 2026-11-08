#pragma once
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "lua.hpp"
#include <flecs.h>

namespace Engine::LuaWrap {

auto RegisterModules(lua_State *state) -> void;

class LuaECSObject : public Object {
public:
  explicit LuaECSObject(flecs::entity entity) : entity(entity) {}

  flecs::entity entity;
};

} // namespace Engine::LuaWrap

namespace LuaWrap {

template <typename T>
concept IsLuaECSObject = std::is_base_of_v<Engine::LuaWrap::LuaECSObject, std::remove_cvref_t<T>>;

inline auto EntityFromLua(lua_State *state, int index) -> flecs::entity * {
  // Check if userdata
  if (lua_isuserdata(state, index) == 0) {
    return nullptr;
  }

  // NOLINTNEXTLINE
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    return nullptr;
  }

  if (proxy->object == nullptr || proxy->type == nullptr) {
    return nullptr;
  }

  // A Camera might have a Transform. Meaning the proxy type is Camera, but T is Transform
  // But because both Camera and Transform are LuaECSObjects, they both have an entity field, so we can still get the entity from the proxy object
  // if (proxy->type->GetName() != T::GetType()->GetName()) {
  //   return nullptr;
  // }

  // NOLINTNEXTLINE
  auto *obj = static_cast<Engine::LuaWrap::LuaECSObject *>(proxy->object);
  return &obj->entity;
}

template <typename T> inline auto ComponentFromLua(lua_State *state, int index) -> Result<T *> {
  auto *entity = EntityFromLua(state, index);
  CHECK_NULL(entity);

  auto *component = entity->try_get_mut<T>();
  CHECK_NULL(component);

  return component;
}

} // namespace LuaWrap