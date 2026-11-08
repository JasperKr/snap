#pragma once

#include <string>
#include <utility>

class Type {
public:
  explicit Type(std::string name) : Name(std::move(name)) {}
  Type(std::string name, const Type &parent) : Name(std::move(name)), Parent(&parent) {}

  [[nodiscard]] auto GetName() const -> const std::string & { return Name; }
  [[nodiscard]] auto GetParent() const -> const Type * { return Parent; }

  auto operator==(const Type &other) const -> bool { return this == &other; }

private:
  std::string Name;
  const Type *Parent{nullptr};
};