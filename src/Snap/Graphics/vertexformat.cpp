#include "vertexformat.hpp"
#include "Graphics/FrameGraph/commands.hpp"

namespace Graphics {

auto VertexFormat::BindDynamicInputState(VirtualCommandBuffer *commandBuffer)
    -> void {
  auto currentHash = GetHash();
  auto &threadContext = GetThreadContext();

  const auto &bindings = GetBindings();
  const auto &attributes = GetVkAttributes2();

  commandBuffer->SetVertexInputEXT(
      {static_cast<uint32_t>(bindings.size()), bindings.data(),
       static_cast<uint32_t>(attributes.size()), attributes.data()});
}

} // namespace Graphics