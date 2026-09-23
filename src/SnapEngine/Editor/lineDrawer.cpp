#include "lineDrawer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Modules/error.hpp"
#include "Scene/camera.hpp"
#include <vulkan/vulkan_core.h>

namespace Engine::Renderer {

void LineDrawer::DrawLine(const Math::Vec3 &start, const Math::Vec3 &end,
                          const Math::PackedColor &color, float thickness) {
  Lines.emplace_back(LineVertex{
      .Start = start, .End = end, .Color = color, .Thickness = thickness});
}

void LineDrawer::OverlayLine(const Math::Vec3 &start, const Math::Vec3 &end,
                             const Math::PackedColor &color, float thickness) {
  OverlayLines.emplace_back(LineVertex{
      .Start = start, .End = end, .Color = color, .Thickness = thickness});
}

void LineDrawer::DrawWireframeBox(const Math::Vec3 &min, const Math::Vec3 &max,
                                  const Math::PackedColor &color,
                                  float thickness) {
  // Draw the edges of the box using lines
  DrawLine(Math::Vec3{min.x, min.y, min.z}, Math::Vec3{max.x, min.y, min.z},
           color, thickness);
  DrawLine(Math::Vec3{max.x, min.y, min.z}, Math::Vec3{max.x, max.y, min.z},
           color, thickness);
  DrawLine(Math::Vec3{max.x, max.y, min.z}, Math::Vec3{min.x, max.y, min.z},
           color, thickness);
  DrawLine(Math::Vec3{min.x, max.y, min.z}, Math::Vec3{min.x, min.y, min.z},
           color, thickness);

  DrawLine(Math::Vec3{min.x, min.y, max.z}, Math::Vec3{max.x, min.y, max.z},
           color, thickness);
  DrawLine(Math::Vec3{max.x, min.y, max.z}, Math::Vec3{max.x, max.y, max.z},
           color, thickness);
  DrawLine(Math::Vec3{max.x, max.y, max.z}, Math::Vec3{min.x, max.y, max.z},
           color, thickness);
  DrawLine(Math::Vec3{min.x, max.y, max.z}, Math::Vec3{min.x, min.y, max.z},
           color, thickness);

  DrawLine(Math::Vec3{min.x, min.y, min.z}, Math::Vec3{min.x, min.y, max.z},
           color, thickness);
  DrawLine(Math::Vec3{max.x, min.y, min.z}, Math::Vec3{max.x, min.y, max.z},
           color, thickness);
  DrawLine(Math::Vec3{max.x, max.y, min.z}, Math::Vec3{max.x, max.y, max.z},
           color, thickness);
  DrawLine(Math::Vec3{min.x, max.y, min.z}, Math::Vec3{min.x, max.y, max.z},
           color,
           thickness); // NOLINT
}

void LineDrawer::OverlayWireframeBox(const Math::Vec3 &min,
                                     const Math::Vec3 &max,
                                     const Math::PackedColor &color,
                                     float thickness) {
  // Draw the edges of the box using lines
  OverlayLine(Math::Vec3{min.x, min.y, min.z}, Math::Vec3{max.x, min.y, min.z},
              color, thickness);
  OverlayLine(Math::Vec3{max.x, min.y, min.z}, Math::Vec3{max.x, max.y, min.z},
              color, thickness);
  OverlayLine(Math::Vec3{max.x, max.y, min.z}, Math::Vec3{min.x, max.y, min.z},
              color, thickness);
  OverlayLine(Math::Vec3{min.x, max.y, min.z}, Math::Vec3{min.x, min.y, min.z},
              color, thickness);

  OverlayLine(Math::Vec3{min.x, min.y, max.z}, Math::Vec3{max.x, min.y, max.z},
              color, thickness);
  OverlayLine(Math::Vec3{max.x, min.y, max.z}, Math::Vec3{max.x, max.y, max.z},
              color, thickness);
  OverlayLine(Math::Vec3{max.x, max.y, max.z}, Math::Vec3{min.x, max.y, max.z},
              color, thickness);
  OverlayLine(Math::Vec3{min.x, max.y, max.z}, Math::Vec3{min.x, min.y, max.z},
              color,
              thickness); // NOLINT

  OverlayLine(Math::Vec3{min.x, min.y, min.z}, Math::Vec3{min.x, min.y, max.z},
              color,
              thickness); // NOLINT
  OverlayLine(Math::Vec3{max.x, min.y, min.z}, Math::Vec3{max.x, min.y, max.z},
              color,
              thickness); // NOLINT
  OverlayLine(Math::Vec3{max.x, max.y, min.z}, Math::Vec3{max.x, max.y, max.z},
              color,
              thickness); // NOLINT
  OverlayLine(Math::Vec3{min.x, max.y, min.z}, Math::Vec3{min.x, max.y, max.z},
              color,
              thickness); // NOLINT
}

auto LineDrawer::Initialize(const Graphics::GraphicsContext &context) -> Error {
  Graphics::MeshCreationInfo info{
      .vertexFormat = &LineVertexFormat,
      .vertexCount = MaxVertexCount,
      .debugName = "Lines mesh",
  };

  Mesh = CHECK_RES(Graphics::Mesh::Create(context, info));

  Shader = CHECK_RES(Graphics::Shader::Create(
      context, "Graphics/Shaders/GUI/lineDrawer", "Line shader"));

  return {};
}

auto LineDrawer::Deinitialize() -> void {
  Mesh = nullptr;
  Shader = nullptr;
}

auto LineDrawer::GenerateMesh(const Graphics::GraphicsContext &context,
                              std::vector<LineVertex> &lines) -> Error {
  // NOLINTNEXTLINE
  auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(lines.data()),
                                 lines.size() * sizeof(LineVertex));

  CHECK_ERR(Mesh->SetVertices(context, 0, span));

  return {};
}

auto LineDrawer::Render(const Graphics::GraphicsContext &context,
                        Camera &camera) -> Error {

  if (Lines.empty() && OverlayLines.empty()) {
    return {};
  }

  Graphics::RenderState::SetShader(Shader);
  Graphics::RenderState::SetDepthMode(true, false, VK_COMPARE_OP_GREATER);

  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};
  CHECK_ERR(Shader->Send(cameraBufferKey, camera.GetBuffer()));

  static auto viewportSizeKey =
      Graphics::ResourceKey{"PushConstants", "viewportSize"};

  CHECK_ERR(Graphics::UniformWriter::Send(Shader, viewportSizeKey,
                                          camera.GetDimensions()));

  Mesh->SetDrawRange({.Offset = 0, .Count = 6}); // NOLINT
  if (!Lines.empty()) {
    CHECK_ERR(GenerateMesh(context, Lines));

    CHECK_ERR(Graphics::Draw(context, *Mesh, Lines.size()));
    Lines.clear();
  }

  if (OverlayLines.empty()) {
    return {};
  }

  CHECK_ERR(GenerateMesh(context, OverlayLines));

  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  CHECK_ERR(Graphics::Draw(context, *Mesh, OverlayLines.size()));
  OverlayLines.clear();

  return {};
}

} // namespace Engine::Renderer