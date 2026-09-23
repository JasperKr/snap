#include "primitiveDrawer.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/uniformWriter.hpp"

namespace Engine::Renderer {

auto PrimitiveDrawer::DrawPrimitive(const std::vector<Math::Vec3> &vertices,
                                    const Math::PackedColor &color) -> void {
  for (const auto &vertex : vertices) {
    Primitives.push_back({vertex, color});
  }
}

auto PrimitiveDrawer::DrawPrimitive(const std::vector<Math::Vec3> &vertices,
                                    const std::vector<uint32_t> &indices,
                                    const Math::PackedColor &color) -> void {
  for (const auto index : indices) {
    Primitives.push_back({vertices[index], color});
  }
}

auto PrimitiveDrawer::DrawPrimitive(const Math::Vec3 &vertex1,
                                    const Math::Vec3 &vertex2,
                                    const Math::Vec3 &vertex3,
                                    const Math::PackedColor &color) -> void {
  Primitives.push_back({vertex1, color});
  Primitives.push_back({vertex2, color});
  Primitives.push_back({vertex3, color});
}

auto PrimitiveDrawer::OverlayPrimitive(const std::vector<Math::Vec3> &vertices,
                                       const Math::PackedColor &color) -> void {
  for (const auto &vertex : vertices) {
    Overlaid.push_back({vertex, color});
  }
}

auto PrimitiveDrawer::OverlayPrimitive(const std::vector<Math::Vec3> &vertices,
                                       const std::vector<uint32_t> &indices,
                                       const Math::PackedColor &color) -> void {
  for (const auto index : indices) {
    Overlaid.push_back({vertices[index], color});
  }
}

auto PrimitiveDrawer::OverlayPrimitive(const Math::Vec3 &vertex1,
                                       const Math::Vec3 &vertex2,
                                       const Math::Vec3 &vertex3,
                                       const Math::PackedColor &color) -> void {
  Overlaid.push_back({vertex1, color});
  Overlaid.push_back({vertex2, color});
  Overlaid.push_back({vertex3, color});
}

auto PrimitiveDrawer::Deinitialize() -> void {
  Mesh = nullptr;
  Shader = nullptr;
}

auto PrimitiveDrawer::Initialize(const Graphics::GraphicsContext &context)
    -> Error {
  Graphics::MeshCreationInfo info{
      .vertexFormat = &PrimitiveVertexFormat,
      .vertexCount = MaxVertexCount,
      .debugName = "Primitives mesh",
  };

  Mesh = CHECK_RES(Graphics::Mesh::Create(context, info));

  Shader = CHECK_RES(
      Graphics::Shader::Create(context, "GUI/primitiveDrawer", "Prim shader"));

  return {};
}

auto PrimitiveDrawer::GenerateMesh(const Graphics::GraphicsContext &context,
                                   std::vector<PrimitiveVertex> &primitives)
    -> Error {
  // NOLINTNEXTLINE
  auto span = std::span<uint8_t>(reinterpret_cast<uint8_t *>(primitives.data()),
                                 primitives.size() * sizeof(PrimitiveVertex));

  CHECK_ERR(Mesh->GetVertexBuffer()->SetData(context, span));

  return {};
}

auto PrimitiveDrawer::Render(const Graphics::GraphicsContext &context,
                             Camera &camera) -> Error {

  if (Primitives.empty() && Overlaid.empty()) {
    return {};
  }

  Graphics::RenderState::SetShader(Shader);
  Graphics::RenderState::SetDepthMode(true, false, VK_COMPARE_OP_GREATER);

  static auto cameraBufferKey = Graphics::ResourceKey{"CameraData"};
  CHECK_ERR(Shader->Send(cameraBufferKey, camera.GetBuffer()));

  if (!Primitives.empty()) {
    CHECK_ERR(GenerateMesh(context, Primitives));

    Mesh->SetDrawRange(
        {.Offset = 0, .Count = static_cast<uint32_t>(Primitives.size())});

    CHECK_ERR(Graphics::Draw(context, *Mesh, Primitives.size()));
    Primitives.clear();
  }

  if (Overlaid.empty()) {
    return {};
  }

  CHECK_ERR(GenerateMesh(context, Overlaid));

  Mesh->SetDrawRange(
      {.Offset = 0, .Count = static_cast<uint32_t>(Overlaid.size())});

  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);
  CHECK_ERR(Graphics::Draw(context, *Mesh));
  Overlaid.clear();

  return {};
}

} // namespace Engine::Renderer