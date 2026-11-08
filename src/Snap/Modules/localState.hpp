#pragma once
#include "Modules/object.hpp"
#include <unordered_map>

template <typename State>
  requires(std::is_default_constructible_v<State>)
class ThreadLocalState {
public:
  template <typename Owner> auto GetState(const Owner &owner) const -> State & { return GetMap()[owner.getID()]; }

  template <typename Owner> auto EraseState(const Owner &owner) -> void { GetMap().erase(owner.getID()); }

private:
  auto static GetMap() -> std::unordered_map<ObjectID, State> & {
    thread_local std::unordered_map<ObjectID, State> map;
    return map;
  }
};