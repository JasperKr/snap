#pragma once

#include "Graphics/graphicsContext.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Scene/camera.hpp"
#include <vector>
namespace Engine::Renderer {

struct BloomManager {
  auto Initialize(const Graphics::GraphicsContext &context) -> Error;
  auto Deinitialize() -> void;

  auto ApplyBloom(const Graphics::GraphicsContext &context, Camera &camera)
      -> Error;
  auto UpdateViews(const Graphics::GraphicsContext &context, Camera &camera)
      -> Error;

private:
  Ref<Graphics::Shader> ThresholdShader;
  Ref<Graphics::Shader> DownsampleShader;
  Ref<Graphics::Shader> UpsampleShader;
  Ref<Graphics::Shader> DirtShader;

  Ref<Graphics::Texture> DirtTexture;

  std::vector<Ref<Graphics::Texture>> DownsampleChainViews;
};

} // namespace Engine::Renderer