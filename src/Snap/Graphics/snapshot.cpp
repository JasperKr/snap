#include "snapshot.hpp"
#include "Graphics/blendmode.hpp"
#include "Graphics/format.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/vkAccessHelpers.hpp"
#include "Modules/Helpers/utils.hpp"
#include "Modules/Math/packedColor.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/console.hpp"
#include "Modules/object.hpp"
#include "Modules/timer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Graphics::Snapshot {

auto GetInternalSnapshot() -> ThreadSnapshot & {
  thread_local ThreadSnapshot currentSnapshot(
      std::vector<std::shared_ptr<::Graphics::Snapshot::Event>>{},
      std::vector<::Graphics::RenderState::State>{}, 0, "", false);
  return currentSnapshot;
}

auto GetCurrentSnapshot() -> ThreadSnapshot * {
  auto &currentSnapshot = GetInternalSnapshot();

  if (!currentSnapshot.active) {
    return nullptr;
  }

  return &currentSnapshot;
}

auto StartSnapshot() -> void {
  auto &currentSnapshot = GetInternalSnapshot();

  currentSnapshot.events.clear();
  currentSnapshot.renderStates.clear();
  currentSnapshot.threadId =
      std::hash<std::thread::id>{}(std::this_thread::get_id());
  currentSnapshot.threadName = Graphics::ContextDebugname;
  currentSnapshot.active = true;
}

auto EndSnapshot() -> void {
  auto &currentSnapshot = GetInternalSnapshot();
  currentSnapshot.active = false;
  // currentSnapshot.renderStates = {};
  // currentSnapshot.threadId = 0;
  // currentSnapshot.threadName = {};
  // currentSnapshot.events = std::vector<std::shared_ptr<Event>>{};
}

inline auto DrawLegendItem(const char *name, ImU32 color) {
  auto *drawList = ImGui::GetWindowDrawList();

  float height = ImGui::GetTextLineHeightWithSpacing();
  const float radius = 7.0F;

  ImVec2 cursor = ImGui::GetCursorScreenPos();

  auto center = cursor;
  center.x += radius;
  center.y += height / 2.0F; // NOLINT

  drawList->AddCircleFilled(center, radius, color);
  drawList->AddCircle(center, radius, HexToImU32(0x2d2d2d)); // NOLINT

  ImGui::SetCursorScreenPos(
      {cursor.x + radius * 2.0F + 4.0F, cursor.y}); // NOLINT
  ImGui::Text("%s", name);
}

auto DrawPipelineStage(VkPipelineStageFlagBits2 stage) {
  auto infoIter = PipelineStageUiInfo.find(stage);
  if (infoIter == PipelineStageUiInfo.end()) {
    return;
  }

  DrawLegendItem(infoIter->second.second, infoIter->second.first);
}

auto DrawPipelineStages(VkPipelineStageFlagBits2 stages) {
  for (auto flag : Utils::BitMaskRange(stages)) {
    DrawPipelineStage(flag);
  }
}

auto DrawShaderStage(VkShaderStageFlagBits stage) {
  auto infoIter = ShaderStageUiInfo.find(stage);
  if (infoIter == ShaderStageUiInfo.end()) {
    return;
  }

  DrawLegendItem(infoIter->second.second, infoIter->second.first);
}

auto DrawShaderStages(VkShaderStageFlagBits stages) {
  for (auto flag : Utils::BitMaskRange(static_cast<uint32_t>(stages))) {
    DrawShaderStage(static_cast<VkShaderStageFlagBits>(flag));
  }
}

enum class TextRenderMode : uint8_t { None, Initials, Full };

auto GetTextRenderMode(float widthAvailable) {
  auto mode = TextRenderMode::None;
  auto singleCharSize = ImGui::CalcTextSize("W").x;

  if (widthAvailable > singleCharSize * 4) {
    mode = TextRenderMode::Full;
  } else if (widthAvailable > singleCharSize) {
    mode = TextRenderMode::Initials;
  }

  return mode;
}

// NOLINTNEXTLINE
auto RenderSnapshot(const ThreadSnapshot &snapshot) -> void {
  int index = 1;

  if (snapshot.events.empty()) {
    return;
  }

  float offset{};
  float lineHeight = ImGui::GetTextLineHeightWithSpacing();

  const ImU32 defaultCol = HexToImU32(0x333333);
  const ImU32 barrierCol = HexToImU32(0x6d1712);
  const ImU32 layoutCol = HexToImU32(0x124d6d);
  const ImU32 endRenderingCol = HexToImU32(0x185615);

  float charWidth = ImGui::CalcTextSize("W").x;

  static std::shared_ptr<const Event> focussedEvent;
  static ObjectID associatedIdentifier;

  if (associatedIdentifier != snapshot.getID()) {
    focussedEvent = nullptr;
  }

  auto &inout = ImGui::GetIO();

  if (ImGui::Begin("Snapshot Viewer")) {
    auto *list = ImGui::GetWindowDrawList();

    static Math::Vec2 cameraPosition;
    static float cameraZoom = 1.0F;

    float widthAvailable = ImGui::GetContentRegionAvail().x;
    float itemSize = widthAvailable /
                     static_cast<float>(std::max(1UL, snapshot.events.size()));

    float yOffset = ImGui::GetCursorPosY();
    float rounding = ImGui::GetStyle().FrameRounding;

    cameraZoom *= 1.0F + (inout.MouseWheel * 0.1F); // NOLINT

    Math::Vec2 currentCameraPosition = cameraPosition;

    currentCameraPosition.x -=
        ImGui::GetMouseDragDelta(ImGuiMouseButton_Right).x / cameraZoom;

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
      cameraPosition = currentCameraPosition;
    }

    ImVec2 mousePosition = ImGui::GetMousePos();

    for (const auto &event : snapshot.events) {
      ImGui::PushID(index++);

      ImVec2 min = {offset, yOffset};
      offset += itemSize;
      ImVec2 size = {itemSize - (1 / cameraZoom), lineHeight};
      ImU32 color = defaultCol;

      if (event->type == EventType::Barrier) {
        min.y += lineHeight;
        color = barrierCol;
      } else if (event->type == EventType::LayoutTransition) {
        min.y += lineHeight * 2;
        color = layoutCol;
      } else if (event->type == EventType::EndRendering) {
        min.y += lineHeight * 3;
        color = endRenderingCol;
      }

      min.x -= currentCameraPosition.x;
      min.x -= ImGui::GetWindowWidth() / 2;
      min.x *= cameraZoom;
      min.x += ImGui::GetWindowWidth() / 2;

      min.x += ImGui::GetWindowPos().x;
      min.y += ImGui::GetWindowPos().y;

      size.x *= cameraZoom;

      ImVec2 max = {min.x + size.x, min.y + size.y};

      list->AddRectFilled(min, max, color, rounding);

      ImGui::SetCursorScreenPos(min);
      ImGui::Dummy(size);
      bool clicked = ImGui::IsItemHovered();

      if (size.x - 4 > charWidth) {
        max.x -= 4;
        min.x += 2;

        list->PushClipRect(min, max, true);

        ImGui::SetCursorScreenPos(min);
        ImGui::Text("%s", EventTypeStringHelper.ToString(event->type).data());

        list->PopClipRect();
      }

      if (clicked) {
        ImGui::BeginTooltip();
        // event->DrawImGui(&snapshot);
        ImGui::Text("%s", EventTypeStringHelper.ToString(event->type).data());
        ImGui::EndTooltip();

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
          focussedEvent = event;
          associatedIdentifier = snapshot.getID();
        }
      }

      ImGui::PopID();
    }
  }
  ImGui::End();

  if (focussedEvent != nullptr) {
    bool isOpen = true;
    if (ImGui::Begin("Event Info", &isOpen)) {
      focussedEvent->DrawImGui(&snapshot);
    }
    ImGui::End();

    if (!isOpen) {
      focussedEvent = nullptr;
    }
  }
}

auto Event::DrawImGui(ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Event type: %s", EventTypeStringHelper.ToString(type).data());
  ImGui::Text("Timestamp: %lu", timestamp);

  DrawVariantImGui(parent);
}

void Event::DrawVariantImGui(ThreadSnapshot const *parent) const {}

Event::~Event() = default;

GraphicsEvent::GraphicsEvent() {
#if Enable_Snapshots
  auto state = *Graphics::RenderState::TopOfStack;
  auto *snapshot = GetCurrentSnapshot();
  if (snapshot != nullptr) {
    renderState = static_cast<int>(snapshot->renderStates.size());
    snapshot->renderStates.emplace_back(state);
  } else {
    renderState = -1;
  }
#else
  renderState = -1; // NOLINT
#endif
}

inline void BooleanFlag(const char *label, bool value,
                        const char *trueLabel = "Enabled", // NOLINT
                        const char *falseLabel = "Disabled") {
  ImGui::Text("%s", label);
  ImGui::SameLine();
  if (value) {
    ImGui::Text("%s", trueLabel);
  } else {
    ImGui::TextDisabled("%s", falseLabel);
  }
}

inline auto DrawRendertargetImGui(const RenderState::RenderTarget &target,
                                  int index) -> void {
  auto name = target.texture->GetDebugName();

  if (ImGui::TreeNode("Rendertarget", "%s", name.data())) {
    ImGui::Text(
        "Format: %s",
        Format::ImageFormatToString(target.texture->GetFormat()).data());

    auto blend = target.blendMode;
    BooleanFlag("Blend Mode:", blend.blendEnable != 0U);

    auto [isDefault, blendmode, alphamode] = BlendMode::ToString(blend);
    if (isDefault) {
      ImGui::Text("Blend Mode: %s, Alpha Mode: %s", blendmode.c_str(),
                  alphamode.c_str());
      ImGui::SetItemTooltip(
          "Blend State:\nSrc-Color=%s\nDst-Color=%s\nColor Op=%s"
          "\nSrc Alpha=%s\nDst Alpha=%s\nAlpha Op=%s",
          BlendMode::ToString(blend.srcColorBlendFactor).data(),
          BlendMode::ToString(blend.dstColorBlendFactor).data(),
          BlendMode::ToString(blend.colorBlendOp).data(),
          BlendMode::ToString(blend.srcAlphaBlendFactor).data(),
          BlendMode::ToString(blend.dstAlphaBlendFactor).data(),
          BlendMode::ToString(blend.alphaBlendOp).data());
    } else {
      ImGui::Text("Blend State:\nSrc-Color=%s\nDst-Color=%s\nColor Op=%s"
                  "\nSrc Alpha=%s\nDst Alpha=%s\nAlpha Op=%s",
                  BlendMode::ToString(blend.srcColorBlendFactor).data(),
                  BlendMode::ToString(blend.dstColorBlendFactor).data(),
                  BlendMode::ToString(blend.colorBlendOp).data(),
                  BlendMode::ToString(blend.srcAlphaBlendFactor).data(),
                  BlendMode::ToString(blend.dstAlphaBlendFactor).data(),
                  BlendMode::ToString(blend.alphaBlendOp).data());
    }

    ImGui::Text(
        "Clear Value: R=%.2f, G=%.2f, B=%.2f, A=%.2f",
        target.clearValue.color.float32[0], target.clearValue.color.float32[1],
        target.clearValue.color.float32[2], target.clearValue.color.float32[3]);
    auto location = target.location;
    if (location == -1) {
      location = index;
    }

    ImGui::Text("Location: %d", location);

    ImGui::TreePop();
  }
}

auto GraphicsEvent::DrawStateImGui(ThreadSnapshot const *parent) const -> void {
  if (renderState < 0) {
    ImGui::Text("No render state captured.");
    return;
  }

  if (parent == nullptr || renderState >= parent->renderStates.size()) {
    ImGui::Text("Invalid render state index.");
    return;
  }

  const auto &state = parent->renderStates[renderState];

  ImGui::Text("Render State:");
  ImGui::Indent();
  ImGui::Text("Shader: %s (Module name: %s)", state.shader->name.c_str(),
              state.shader->moduleName.c_str());
  DrawShaderStage(state.shader->combinedShaderStages);
  ImGui::Text("Rendertargets:");
  ImGui::Indent();
  for (int i = 0; i < state.colorAttachments.size(); i++) {
    const auto &target = state.colorAttachments.at(i);
    ImGui::PushID(i);
    DrawRendertargetImGui(target, i);
    ImGui::PopID();
  }
  if (state.hasDepthStencilAttachment) {
    ImGui::PushID("DepthStencil");
    DrawRendertargetImGui(state.depthStencilAttachment, 0);
    ImGui::PopID();
  }
  ImGui::Unindent();

  std::string cullmodeStr;
  switch (state.cullMode) {
  case VK_CULL_MODE_NONE:
    cullmodeStr = "None";
    break;
  case VK_CULL_MODE_FRONT_BIT:
    cullmodeStr = "Front";
    break;
  case VK_CULL_MODE_BACK_BIT:
    cullmodeStr = "Back";
    break;
  case VK_CULL_MODE_FRONT_AND_BACK:
    cullmodeStr = "All";
    break;
  default:
    cullmodeStr = "Unknown";
    break;
  }
  ImGui::Text("Cull Mode: %s", cullmodeStr.c_str());

  std::string polygonModeStr;
  switch (state.frontFace) {
  case VK_FRONT_FACE_COUNTER_CLOCKWISE:
    polygonModeStr = "Counter Clockwise";
    break;
  case VK_FRONT_FACE_CLOCKWISE:
    polygonModeStr = "Clockwise";
    break;
  case VK_FRONT_FACE_MAX_ENUM:
    break;
  }
  ImGui::Text("Front Face: %s", polygonModeStr.c_str());

  BooleanFlag("Depth Test:", state.depthTestEnable != 0U);
  BooleanFlag("Depth Write:", state.depthWriteEnable != 0U);
  BooleanFlag("Stencil Test:", state.stencilTestEnable != 0U);

  std::string topologyStr;
  switch (state.primitiveTopology) {
  case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
    topologyStr = "Point List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    topologyStr = "Line List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    topologyStr = "Line Strip";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    topologyStr = "Triangle List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    topologyStr = "Triangle Strip";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    topologyStr = "Triangle Fan";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
    topologyStr = "Line List with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
    topologyStr = "Line Strip with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
    topologyStr = "Triangle List with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
    topologyStr = "Triangle Strip with Adjacency";
    break;
  case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
    topologyStr = "Patch List";
    break;
  case VK_PRIMITIVE_TOPOLOGY_MAX_ENUM:
    break;
  }
  ImGui::Text("Primitive Topology: %s", topologyStr.c_str());
  if (state.hasViewport) {
    ImGui::Text("Viewport: x=%.2f, y=%.2f, width=%.2f, height=%.2f",
                state.viewport.x, state.viewport.y, state.viewport.width,
                state.viewport.height);
  } else {
    if (!state.colorAttachments.empty() &&
        state.colorAttachments.at(0).texture) {
      auto texture = state.colorAttachments.at(0).texture;
      ImGui::Text("Viewport: x=0.00, y=0.00, width=%.2u, height=%.2u (Default)",
                  texture->GetWidth(), texture->GetHeight());
    } else {
      ImGui::Text("Viewport: Unknown");
    }
  }

  if (state.hasScissor) {
    ImGui::Text("Scissor: x=%u, y=%u, width=%u, height=%u",
                state.scissor.offset.x, state.scissor.offset.y,
                state.scissor.extent.width, state.scissor.extent.height);
  } else {
    if (!state.colorAttachments.empty() &&
        state.colorAttachments.at(0).texture) {
      auto texture = state.colorAttachments.at(0).texture;
      ImGui::Text("Scissor: x=0, y=0, width=%u, height=%u (Default)",
                  texture->GetWidth(), texture->GetHeight());
    } else {
      ImGui::Text("Scissor: Unknown");
    }
  }
};

auto BufferCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Size: %lu", size);
  ImGui::Text("Usage: %u", usage);
  ImGui::Text("Properties: %u", properties);
};

auto BufferDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
};

auto BufferUploadEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Size: %lu", size);
  ImGui::Text("Data Size: %zu bytes", data.size());
};

inline auto
DrawComponentData(const std::span<const uint8_t> &data,
                  const BufferComponent &component, // NOLINTNEXTLINE
                  VkDeviceSize offset, VkDeviceSize rawOffset) -> void {
  auto format = std::get<VkFormat>(component.format);
  auto formatSize = Format::GetSize(format);

  bool withinSpan = offset + (formatSize * component.arraySize) <= data.size();
  if (offset < rawOffset || !withinSpan) {
    ImGui::Text("No data available.");
    return;
  }

  auto span = Utils::Subspan(data, offset, formatSize * component.arraySize);

  if (component.arraySize > 1) {
    if (component.isMatrix) {
      auto rowCount = component.arraySize;
      auto columnCount = Format::GetChannelCount(format);

      const auto &firstRow = Format::ToString(format, span.data());
      ImGui::Text("/ %s \\", firstRow.c_str());

      for (size_t row = 1; row < rowCount - 1; row++) {
        auto rowSpan = Utils::Subspan(span, row * formatSize, formatSize);
        auto rowStr = Format::ToString(format, rowSpan.data());
        ImGui::Text("| %s |", rowStr.c_str());
      }

      if (rowCount > 1) {
        auto lastRowSpan =
            Utils::Subspan(span, (rowCount - 1) * formatSize, formatSize);
        auto lastRowStr = Format::ToString(format, lastRowSpan.data());
        ImGui::Text("\\ %s /", lastRowStr.c_str());
      }
    } else {
      std::string listStr;
      auto channelCount = Format::GetChannelCount(format);

      for (size_t i = 0; i < component.arraySize; i++) {
        auto elementSpan = Utils::Subspan(span, i * formatSize, formatSize);
        auto elementStr = Format::ToString(format, elementSpan.data());

        if (channelCount == 1) {
          listStr += "  " + elementStr;
        } else {
          listStr += "  (" + elementStr + ")";
        }

        listStr += ";\n";
      }

      ImGui::Text("[#%zu:\n%s]", component.arraySize, listStr.c_str());
    }
  } else {
    auto elementStr = Format::ToString(format, span.data());
    ImGui::Text("%s", elementStr.c_str());
  }
}

inline auto DrawBufferData(const std::span<const uint8_t> &data,
                           const BufferFormat &format,
                           VkDeviceSize offset, // NOLINTNEXTLINE
                           VkDeviceSize size, VkDeviceSize rawOffset) -> void {

  if (offset + size > data.size()) {
    ImGui::Text("No more data available.");
    return;
  }

  for (const auto &component : format.GetComponents()) {
    auto componentOffset = component.offset + offset;

    if (componentOffset >= rawOffset + size) {
      ImGui::TextColored(ImVec4(1.0F, 0.0F, 0.0F, 1.0F),
                         "No more data available.");
      break;
    }

    ImGui::Text("%s =", component.name.c_str());
    ImGui::SameLine();

    if (std::holds_alternative<VkFormat>(component.format)) {
      DrawComponentData(data, component, componentOffset, rawOffset);
    } else {
      auto nestedFormat = std::get<BufferFormat>(component.format);

      for (int element = 0; element < component.arraySize; element++) {
        DrawBufferData(data, nestedFormat, componentOffset,
                       nestedFormat.GetStride(), rawOffset);
      }
    }
  }
};

auto StructuredBufferUploadEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Stride: %lu", format.GetStride());

  if (ImGui::TreeNode("Extra Info")) {
    ImGui::Text("Memory Handle: %p", bufferHandle);
    ImGui::Text("Format: %s", format.ToString().c_str());
    ImGui::TreePop();
  }

  if (!hasAssociatedUploadEvent) {
    ImGui::Text("No associated BufferUploadEvent found.");
    return;
  }

  auto uploadEvent = this->uploadEvent;
  if (uploadEvent.size % format.GetStride() != 0) {
    ImGui::Text("Warning: Structured data upload of size (%lu) is "
                "not a multiple of format stride (%lu).",
                uploadEvent.size, format.GetStride());
  }

  ImGui::Separator();

  uploadEvent.DrawVariantImGui(parent);

  ImGui::Separator();

  if (ImGui::TreeNode("Buffer Data")) {
    DrawBufferData(uploadEvent.data, format, 0, uploadEvent.size,
                   uploadEvent.offset);
    ImGui::TreePop();
  }
};

auto BufferCopyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Src Buffer Handle: %p", srcBufferHandle);
  ImGui::Text("Dst Buffer Handle: %p", dstBufferHandle);
  ImGui::Text("Src Offset: %lu", srcOffset);
  ImGui::Text("Dst Offset: %lu", dstOffset);
  ImGui::Text("Size: %lu", size);
};

auto TextureCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Texture Handle: %p", textureHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Width: %u", width);
  ImGui::Text("Height: %u", height);
  ImGui::Text("Depth: %u", depth);
  ImGui::Text("Mip Levels: %u", mipLevels);
  ImGui::Text("Array Layers: %u", arrayLayers);
  ImGui::Text("Format: %s",
              Format::ToString(static_cast<VkFormat>(format)).data());
  ImGui::Text("Usage: %u", usage);
  ImGui::Text("Properties: %u", properties);
};

auto TextureDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Texture Handle: %p", textureHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
};

auto TextureUploadEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Texture Handle: %p", textureHandle);
  ImGui::Text("Memory Handle: %p", memoryHandle);
  ImGui::Text("Width: %u", width);
  ImGui::Text("Height: %u", height);
  ImGui::Text("Depth: %u", depth);
  ImGui::Text("Mip Level: %u", mipLevel);
  ImGui::Text("Array Layer: %u", arrayLayer);
  ImGui::Text("Format: %s",
              Format::ToString(static_cast<VkFormat>(format)).data());
  ImGui::Text("Data Size: %zu bytes", dataSize);
};

auto TextureCopyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Src Texture Handle: %p", srcTextureHandle);
  ImGui::Text("Dst Texture Handle: %p", dstTextureHandle);
  ImGui::Text("Src Width: %u", srcWidth);
  ImGui::Text("Src Height: %u", srcHeight);
};

auto PipelineCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Pipeline Handle: %p", pipelineHandle);
  ImGui::Text("Pipeline Type: %u", pipelineType);
};

auto PipelineDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Pipeline Handle: %p", pipelineHandle);
};

auto DescriptorSetCreateEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Descriptor Set Handle: %p", descriptorSetHandle);
};

auto DescriptorSetDestroyEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Descriptor Set Handle: %p", descriptorSetHandle);
};

auto SamplerCreateEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Sampler Handle: %p", samplerHandle);
};

auto SamplerDestroyEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Sampler Handle: %p", samplerHandle);
};

auto ShaderModuleCreateEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Shader Module Handle: %p", shaderModuleHandle);
  ImGui::Text("Module Name: %s", moduleName.c_str());
};

auto ShaderModuleDestroyEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Shader Module Handle: %p", shaderModuleHandle);
};

auto DrawEvent::DrawVariantImGui(ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Vertex Count: %u", vertexCount);
  ImGui::Text("Instance Count: %u", instanceCount);
  ImGui::Text("First Vertex: %u", firstVertex);
  ImGui::Text("First Instance: %u", firstInstance);

  DrawStateImGui(parent);
};

auto DrawIndexedEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Index Count: %u", indexCount);
  ImGui::Text("Instance Count: %u", instanceCount);
  ImGui::Text("First Index: %u", firstIndex);
  ImGui::Text("Vertex Offset: %d", vertexOffset);
  ImGui::Text("First Instance: %u", firstInstance);

  DrawStateImGui(parent);
};

auto DrawIndirectEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Indirect Buffer Handle: %p", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Draw Count: %u", drawCount);
  ImGui::Text("Stride: %u", stride);

  DrawStateImGui(parent);
};

auto DrawIndexedIndirectEvent::DrawVariantImGui(
    ThreadSnapshot const *parent) const -> void {
  ImGui::Text("Indirect Buffer Handle: %p", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Draw Count: %u", drawCount);
  ImGui::Text("Stride: %u", stride);

  DrawStateImGui(parent);
};

auto DispatchEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Group Count X: %u", groupCountX);
  ImGui::Text("Group Count Y: %u", groupCountY);
  ImGui::Text("Group Count Z: %u", groupCountZ);

  DrawStateImGui(parent);
};

auto DispatchIndirectEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Indirect Buffer Handle: %p", indirectBufferHandle);
  ImGui::Text("Offset: %lu", offset);

  DrawStateImGui(parent);
};

auto SetPipelineEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Pipeline Handle: %p", pipelineHandle);
  ImGui::Text("Pipeline Bind Point: %u", pipelineBindPoint);
};

auto SetDescriptorSetEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Descriptor Set Handle: %p", descriptorSetHandle);
  ImGui::Text("Set Index: %u", setIndex);
};

auto SetVertexBufferEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Offset: %lu", offset);
};

auto SetIndexBufferEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Buffer Handle: %p", bufferHandle);
  ImGui::Text("Offset: %lu", offset);
  ImGui::Text("Index Type: %u", indexType);
};

inline auto ImageLayoutToString(VkImageLayout layout) -> std::string_view {
  // clang-format off
  switch (layout) {
  case VK_IMAGE_LAYOUT_UNDEFINED: return "Undefined";
  case VK_IMAGE_LAYOUT_GENERAL: return "General";
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "Color attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "Depth stencil attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "Depth stencil read only optimal";
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "Shader read only optimal";
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "Transfer src optimal";
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "Transfer dst optimal";
  case VK_IMAGE_LAYOUT_PREINITIALIZED: return "Preinitialized";
  case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return "Depth read only stencil attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL: return "Depth attachment stencil read only optimal";
  case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return "Depth attachment optimal";
  case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return "Depth read only optimal";
  case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return "Stencil attachment optimal";
  case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return "Stencil read only optimal";
  case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return "Read only optimal";
  case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return "Attachment optimal";
  case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ: return "Rendering local read";
  case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "Present src khr";
  case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR: return "Video decode dst khr";
  case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR: return "Video decode src khr";
  case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR: return "Video decode dpb khr";
  case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR: return "Shared present khr";
  case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT: return "Fragment density map optimal ext";
  case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR: return "Fragment shading rate attachment optimal khr";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR: return "Video encode dst khr";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR: return "Video encode src khr";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR: return "Video encode dpb khr";
  case VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT: return "Attachment feedback loop optimal ext";
  case VK_IMAGE_LAYOUT_TENSOR_ALIASING_ARM: return "Tensor aliasing arm";
  case VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR: return "Video encode quantization map khr";
  case VK_IMAGE_LAYOUT_ZERO_INITIALIZED_EXT: return "Zero initialized ext";
  case VK_IMAGE_LAYOUT_MAX_ENUM: return "Max enum";
  }
  // clang-format on
}

auto LayoutTransitionEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
    -> void {
  ImGui::Text("Layout: %s -> %s", ImageLayoutToString(srcLayout).data(),
              ImageLayoutToString(dstLayout).data());

  ImGui::SeparatorText("Source access mask");
  for (const auto &access : Utils::BitMaskRange(srcAccessMask)) {
    ImGui::Text("%s", VkAccessHelpers::AccessFlags2ToString(access).c_str());
  }

  ImGui::SeparatorText("Destination access mask");
  for (const auto &access : Utils::BitMaskRange(dstAccessMask)) {
    ImGui::Text("%s", VkAccessHelpers::AccessFlags2ToString(access).c_str());
  }

  ImGui::SeparatorText("Source pipeline stages");
  DrawPipelineStages(srcStageMask);
  ImGui::SeparatorText("Destination pipeline stages");
  DrawPipelineStages(dstStageMask);
}

// auto BarrierEvent::DrawVariantImGui(ThreadSnapshot const *parent) const
//     -> void {
//   ImGui::Text("Acting on resource: %lu", resourceId);
//   ImGui::Text("Source Stages:");
//   ImGui::Indent();
//   if (sync.srcStages == 0) {
//     ImGui::Text("None");
//   } else {
//     DrawPipelineStages(sync.srcStages);
//   }
//   ImGui::Unindent();
//   ImGui::Text("Destination Stages:");
//   ImGui::Indent();
//   if (sync.dstStages == 0) {
//     ImGui::Text("None");
//   } else {
//     DrawPipelineStages(sync.dstStages);
//   }
//   ImGui::Unindent();

//   ImGui::Text("Source Access:");
//   ImGui::Indent();
//   if (sync.srcAccess == 0) {
//     ImGui::Text("None");
//   } else {
//     for (const auto &access : Utils::BitMaskRange(sync.srcAccess)) {
//       ImGui::Text("%s", AccessFlag2ToString(access));
//     }
//   }
//   ImGui::Unindent();
//   ImGui::Text("Destination Access:");
//   ImGui::Indent();
//   if (sync.dstAccess == 0) {
//     ImGui::Text("None");
//   } else {
//     for (const auto &access : Utils::BitMaskRange(sync.dstAccess)) {
//       ImGui::Text("%s", AccessFlag2ToString(access));
//     }
//   }
//   ImGui::Unindent();
// };

//
auto Update() -> void {
#if Enable_Snapshots
  static double time = 0.0F;
  static const double WarningInterval = 30.0F;

  time -= Timer::GetDelta();

  if (time < 0.0F) {
    time += WarningInterval;

    PrintWarning("Snapshots are enabled; Performance penalty.");
  }
#endif
}

//
} // namespace Graphics::Snapshot