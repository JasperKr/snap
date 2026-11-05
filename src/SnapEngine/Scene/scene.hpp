#pragma once
#include "Graphics/graphicsContext.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/type.hpp"
#include "Scene/Geometry/geometry.hpp"
#include "Wrap/wrap.hpp"
#include "material.hpp"
#include <flecs.h>
#include <lua.hpp>
#include <string>
#include <vector>

struct DrawGuiFuncStorage {
  std::function<void(flecs::entity)> func;
  bool defaultOpen;
};

template <typename T> using DrawGuiFunction = std::function<void(T *, flecs::entity)>;

namespace Engine {

static const Type SceneType = Type("Scene");

struct DrawItem {
  flecs::entity geom_entity;
  struct Geometry geometry;
  Renderer::Material const *material{};
  uint32_t transformIndex{};

  uint64_t primaryKey{};
  uint64_t secondaryKey{};
  uint64_t tertiaryKey{};
};

struct Scene : Object {
  flecs::world world;
  std::vector<flecs::system> preRender;
  flecs::system finalizePreRenderUploads;
  std::string name;
  Error lastUpdateResult = Error::Success();
  flecs::entity currentEnvironment;
  std::unordered_map<flecs::id_t, DrawGuiFuncStorage> drawFunctions;

  std::vector<DrawItem> OpaqueDrawItems;
  std::vector<DrawItem> MaskedDrawItems;
  std::vector<DrawItem> TransparentDrawItems;

  explicit Scene();
  explicit Scene(std::string name);

  static auto GetType() -> const Type * { return &SceneType; }
  auto GetInstanceType() const -> const Type * override { return &SceneType; }

  auto DrawUiElement() const -> Error;
  auto DrawOpaqueModels(const Graphics::GraphicsContext &context, struct Camera &camera) -> Error;
  auto DrawTransparentModels(const Graphics::GraphicsContext &context, struct Camera &camera) -> Error;
  auto PrepareDrawLists(const Graphics::GraphicsContext &context, struct Frustum &frustum) -> Error;
  auto Update(double deltaTime) const -> Error;

  auto UpdateTransforms() const -> void;
  auto UpdateBoundingBoxes() const -> void;

  auto SetEnvironment(flecs::entity environment) -> void;
  auto GetEnvironment() const -> flecs::entity;

private:
  template <typename T> auto AddGuiMethod(const DrawGuiFunction<T> &drawFunction, bool defaultOpen = false) -> void {
    drawFunctions[world.component<T>().id()] =
        DrawGuiFuncStorage{.func = [drawFunction](flecs::entity entity) -> auto { drawFunction(entity.try_get_mut<T>(), entity); }, .defaultOpen = defaultOpen};
  }
};

struct LuaScene : Object {
  explicit LuaScene(Ref<Scene> &scene) : scene(scene) {}

  Ref<Scene> scene;

  static auto GetType() -> const Type * { return &SceneType; }
  [[nodiscard]] auto GetInstanceType() const -> const Type * override { return LuaScene::GetType(); }

  static auto LoadBinding(lua_State *state) -> int;
  static auto DrawUiElement(lua_State *state) -> int;
  static auto Update(lua_State *state) -> int;

  static auto GetName(lua_State *state) -> int;
  static auto SetName(lua_State *state) -> int;

  static auto SetEnvironment(lua_State *state) -> int;
};

extern const ::LuaWrap::LuaClass SceneLuaClass;

} // namespace Engine