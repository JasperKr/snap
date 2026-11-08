#pragma once

#include "Editor/lineDrawer.hpp"
#include "Modules/Math/vector.hpp"
namespace Engine::Renderer {

const std::vector<Graphics::VertexComponent> PrimitiveVertexComponents = {
    Graphics::VertexComponent{
        .name = "Position",
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::VertexComponent{
        .name = "Color",
        .location = 1,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    },
};

const Graphics::VertexFormat PrimitiveVertexFormat(PrimitiveVertexComponents);

struct PrimitiveDrawer {
  struct PrimitiveVertex {
    Math::Vec3 Position;
    Math::PackedColor Color;
  };

  static constexpr size_t MaxPrimitiveCount = 1e5;
  static constexpr size_t MaxVertexCount = MaxPrimitiveCount * 3;

  void DrawPrimitive(const std::vector<Math::Vec3> &vertices, const Math::PackedColor &color);

  void DrawPrimitive(const std::vector<Math::Vec3> &vertices, const std::vector<uint32_t> &indices, const Math::PackedColor &color);

  void DrawPrimitive(const Math::Vec3 &vertex1, const Math::Vec3 &vertex2, const Math::Vec3 &vertex3, const Math::PackedColor &color);

  void OverlayPrimitive(const std::vector<Math::Vec3> &vertices, const Math::PackedColor &color);

  void OverlayPrimitive(const std::vector<Math::Vec3> &vertices, const std::vector<uint32_t> &indices, const Math::PackedColor &color);

  void OverlayPrimitive(const Math::Vec3 &vertex1, const Math::Vec3 &vertex2, const Math::Vec3 &vertex3, const Math::PackedColor &color);

  auto Initialize(const Graphics::GraphicsContext &context) -> Error;
  auto Deinitialize() -> void;

  auto Render(const Graphics::GraphicsContext &context, Camera &camera) -> Error;

private:
  auto GenerateMesh(const Graphics::GraphicsContext &context, std::vector<PrimitiveVertex> &primitives) -> Error;

  std::vector<PrimitiveVertex> Overlaid;
  std::vector<PrimitiveVertex> Primitives;

  Ref<Graphics::Mesh> Mesh;
  Ref<Graphics::Shader> Shader;
};

} // namespace Engine::Renderer