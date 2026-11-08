#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Math/packedColor.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/object.hpp"
#include "Scene/camera.hpp"
#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Engine::Renderer {

const std::vector<Graphics::VertexComponent> LineVertexComponents = {
    Graphics::VertexComponent{
        .name = "Start",
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::VertexComponent{
        .name = "End",
        .location = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
    },
    Graphics::VertexComponent{
        .name = "Color",
        .location = 2,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
    },
    Graphics::VertexComponent{
        .name = "Thickness",
        .location = 3,
        .format = VK_FORMAT_R32_SFLOAT,
    },
};

const Graphics::VertexFormat LineVertexFormat = [] -> Graphics::VertexFormat {
  auto vtxFormat = Graphics::VertexFormat{LineVertexComponents};
  vtxFormat.SetInputRate(0, VK_VERTEX_INPUT_RATE_INSTANCE);

  return vtxFormat;
}();

struct LineDrawer {
  struct LineVertex {
    Math::Vec3 Start;
    Math::Vec3 End;
    Math::PackedColor Color;
    float Thickness{};
  };

  static constexpr size_t MaxLineCount = 1e5;
  static constexpr size_t MaxVertexCount = MaxLineCount * 6;

  void DrawLine(const Math::Vec3 &start, const Math::Vec3 &end, const Math::PackedColor &color, float thickness);

  void OverlayLine(const Math::Vec3 &start, const Math::Vec3 &end, const Math::PackedColor &color, float thickness);

  void DrawWireframeBox(const Math::Vec3 &min, const Math::Vec3 &max, const Math::PackedColor &color, float thickness);

  void OverlayWireframeBox(const Math::Vec3 &min, const Math::Vec3 &max, const Math::PackedColor &color, float thickness);

  auto Initialize(const Graphics::GraphicsContext &context) -> Error;
  auto Deinitialize() -> void;

  auto Render(const Graphics::GraphicsContext &context, Camera &camera) -> Error;

private:
  auto GenerateMesh(const Graphics::GraphicsContext &context, std::vector<LineVertex> &lines) -> Error;

  std::vector<LineVertex> OverlayLines;
  std::vector<LineVertex> Lines;

  Ref<Graphics::Mesh> Mesh;
  Ref<Graphics::Shader> Shader;
};

} // namespace Engine::Renderer