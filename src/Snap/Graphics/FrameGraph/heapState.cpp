#include "heapState.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/shader.hpp"
#include "Libraries/vma.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include <cassert>
#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Graphics {
auto BindGlobalUBO(const GraphicsContext &context, const Ref<Shader> &shader,
                   VkCommandBuffer commandBuffer) -> Error {

  auto &buffer = GetGlobalUniformBuffer(context.frameIndex);
  auto offset = buffer.GetOffset();

  ERR_ASSERT(offset < UINT32_MAX);
  CHECK_ERR(buffer.Write(shader->globalUniforms));

  assert(buffer.GetBuffer().get() != nullptr);

  auto address = CHECK_RES(
      buffer.GetBuffer()->GetAddress(context, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     offset, shader->globalUniforms.size()));

  VkPushDataInfoEXT pushInfo{
      .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
      .pNext = nullptr,
      .offset = 0,
      .data{
          .address = &address,
          .size = sizeof(uint32_t),
      },
  };

  vkCmdPushDataEXT(commandBuffer, &pushInfo);

  return {};
}
} // namespace Graphics