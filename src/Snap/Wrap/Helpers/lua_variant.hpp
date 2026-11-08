#pragma once

#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "lua.hpp"
#include <variant>

namespace LuaWrap {

template <typename Variant> auto PushVariant(lua_State *state, const Variant &value) -> Error {
  std::visit(
      [&](const auto &value) -> auto {
        using T = std::decay_t<decltype(value)>;
        PushObject(state, T::GetType(), value.get());
      },
      value);

  return Error::Success();
}

template <typename Variant, std::size_t... I> inline auto VariantFromLua(lua_State *state, int index) -> Result<Variant> {
  using V = Variant;
  auto *proxy = static_cast<Proxy *>(lua_touserdata(state, index));
  if (proxy == nullptr) {
    return Error::Unexpected("Proxy is null at index " + std::to_string(index));
  }

  Variant result;
  bool matched = false;

  (void)((... || ([&] -> auto {
            using T = std::variant_alternative_t<I, V>;
            if (!matched && *proxy->type == *Ref<T>::GetType()) {
              result = V{*static_cast<Ref<T> *>(proxy->object)};
              matched = true;
              return true;
            }
            return false;
          }())));

  if (!matched) {
    return Error::Unexpected("No matching type found for proxy at index " + std::to_string(index));
  }

  return result;
}

} // namespace LuaWrap