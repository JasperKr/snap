#include "lua.hpp"
#include <utility>

namespace LuaWrap {

struct LuaRef {
  lua_State *state = nullptr;
  int ref = LUA_NOREF;

  LuaRef() = default;

  LuaRef(lua_State *luaState, int index) : state(luaState) {
    lua_pushvalue(state, index);              // copy the value
    ref = luaL_ref(state, LUA_REGISTRYINDEX); // pops and stores NOLINT )
  }

  // Create from top of stack
  static auto FromStack(lua_State *state) -> LuaRef {
    LuaRef reference;
    reference.state = state;
    reference.ref = luaL_ref(state, LUA_REGISTRYINDEX);
    return reference;
  }

  LuaRef(const LuaRef &) = delete; // noncopyable
  auto operator=(const LuaRef &) -> LuaRef & = delete;

  LuaRef(LuaRef &&other) noexcept { *this = std::move(other); }

  auto operator=(LuaRef &&other) noexcept -> LuaRef & {
    if (this != &other) {
      reset();
      state = other.state;
      ref = other.ref;
      other.ref = LUA_NOREF;
    }
    return *this;
  }

  [[nodiscard]] auto valid() const -> bool { return ref != LUA_NOREF && ref != LUA_REFNIL; }

  // Push the stored value onto the stack
  void push() const {
    if (!valid()) {
      return;
    }
    lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
  }

  // Release reference
  void reset() {
    if (valid()) {
      luaL_unref(state, LUA_REGISTRYINDEX, ref);
    }
    ref = LUA_NOREF;
  }

  ~LuaRef() { reset(); }
};
} // namespace LuaWrap