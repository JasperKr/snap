#pragma once

#include "Graphics/graphicsState.hpp"
#include "Modules/Helpers/LRUCache.hpp"
#include "Modules/Helpers/hasher.hpp"
#include "Modules/error.hpp"
#include "Modules/object.hpp"
#include "Modules/stackVector.hpp"
#include "Modules/type.hpp"
#include "graphicsContext.hpp"
#include "shader.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace Graphics::RenderState {

struct Stats {
  uint64_t drawCalls = 0;
  uint64_t dispatchCalls = 0;
  uint64_t triangleCount = 0;
  uint64_t instanceCount = 0;
  uint64_t contextSwitches = 0;

  void Reset() {
    drawCalls = 0;
    dispatchCalls = 0;
    triangleCount = 0;
    instanceCount = 0;
    contextSwitches = 0;
  }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

extern thread_local Stats CurrentStats;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

auto CompareVkPipelineColorBlendAttachmentState(
    const VkPipelineColorBlendAttachmentState &first,
    const VkPipelineColorBlendAttachmentState &second) -> bool;

const static Type LuaRendertargetType = Type("RenderTarget");

struct RenderTarget {
  VkPipelineColorBlendAttachmentState blendMode = BlendmodeNone;
  VkClearValue clearValue = {};
  Ref<Texture> texture;
  int location = -1; // Default to index in the render target array
  mutable bool dirty = true;

  VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  mutable uint64_t hash;
  auto GetHash() const -> uint64_t;

  auto operator==(const RenderTarget &other) const -> bool {
    if (texture->getID() != other.texture->getID()) {
      return false;
    }

    return location == other.location;
  }
};

struct LuaRenderTarget : Object {
  explicit LuaRenderTarget(RenderTarget renderTarget)
      : renderTarget(std::move(renderTarget)) {}

  RenderTarget renderTarget;

  static auto GetType() -> Type const * { return &LuaRendertargetType; }

  [[nodiscard]] auto GetInstanceType() const -> Type const * override {
    return LuaRenderTarget::GetType();
  }
};

struct SetBindingEntry {
  uint32_t setIndex;
  uint32_t binding;
  std::string name;
};

struct State {
  VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
  VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
  VkBool32 depthTestEnable = 1;
  VkBool32 depthWriteEnable = 1;
  VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
  VkBool32 stencilTestEnable = 0;
  VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
  VkViewport viewport{};
  VkRect2D scissor{};

  bool hasViewport = false;
  bool hasScissor = false;
  mutable bool dirty = true;

  Math::StackVector<VkColorBlendEquationEXT, MAX_COLOR_ATTACHMENTS>
      colorBlendEquations;

  Ref<Shader> shader;

  VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  Math::StackVector<RenderTarget, MAX_COLOR_ATTACHMENTS> colorAttachments;
  RenderTarget depthStencilAttachment;
  bool hasDepthStencilAttachment = false;

  mutable uint64_t hash{};

  // Incremented each time the state is modified
  mutable uint64_t generation = 0;

  void MarkUpdated() {
    generation++;
    dirty = true;
  }

  auto GetHash() const -> uint64_t;

  auto operator==(const State &other) const -> bool {
    [[likely]]
    if (generation == other.generation) { // quick equal
      return true;
    }

    if (colorAttachments != other.colorAttachments) {
      return false;
    }

    if (shader != other.shader) {
      return false;
    }

    if (stencilTestEnable != other.stencilTestEnable ||
        polygonMode != other.polygonMode ||
        primitiveTopology != other.primitiveTopology ||
        bindPoint != other.bindPoint) {
      return false;
    }

    if (hasDepthStencilAttachment != other.hasDepthStencilAttachment) {
      return false;
    }

    if (hasDepthStencilAttachment) {
      if (depthStencilAttachment != other.depthStencilAttachment) {
        return false;
      }
    }

    return true;
  }

  auto ToString() const -> std::string;
};

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local std::vector<State> StateStack;

extern thread_local State LastStateStorage;
extern thread_local State *LastState;
extern thread_local State *TopOfStack;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline auto HashRenderTarget(const RenderTarget &renderTarget) -> size_t {
  Hash::Hasher hasher{};

  hasher.Add(renderTarget.texture->getID());
  hasher.Add(renderTarget.location);

  return hasher.Get();
}

auto FinalizeFrame(const GraphicsContext &context) -> Error;
auto BeginFrame(const GraphicsContext &context) -> Error;

auto Push(const GraphicsContext &context) -> Error;
auto Pop(const GraphicsContext &context) -> Error;
auto Reset(const GraphicsContext &context) -> Error;
auto Load(const GraphicsContext &context) -> Error;

// Destroys all created pipelines and layouts
void Destroy(const GraphicsContext &context);

// Shuts down the local dynamic rendering module
void Shutdown(const GraphicsContext &context);

auto SetDepthMode(bool enable, bool writeEnable, VkCompareOp compareOp) -> void;
auto SetCullMode(VkCullModeFlags cullMode) -> void;
auto SetPolygonMode(VkPolygonMode polygonMode) -> void;
auto SetViewport(const VkViewport *viewport) -> void;
auto SetScissor(const VkRect2D *scissor) -> void;
auto ClipScissor(const VkRect2D &scissor) -> void;
auto SetShader(const Ref<Shader> &shader) -> void;
auto SetRenderTargets(const GraphicsContext &context,
                      const std::vector<RenderTarget> &renderTargets) -> Error;
auto SetWindingOrder(VkFrontFace frontFace) -> void;
auto SetTopology(VkPrimitiveTopology topology) -> void;

auto GetDepthMode() -> std::tuple<bool, bool, VkCompareOp>;
auto GetCullMode() -> VkCullModeFlags;
auto GetPolygonMode() -> VkPolygonMode;
auto GetViewport() -> VkViewport;
auto GetClippedViewport() -> VkViewport;
auto GetMaximumAllowedViewport() -> VkViewport;
auto GetScissor() -> VkRect2D;
auto GetUserShader() -> Ref<Shader>;
auto GetShader() -> Ref<Shader>;
auto GetRenderTargets() -> std::vector<RenderTarget>;
auto GetWindingOrder() -> VkFrontFace;
auto GetTopology() -> VkPrimitiveTopology;
auto SetBindPoint(VkPipelineBindPoint bindPoint) -> void;
auto GetBindPoint() -> VkPipelineBindPoint;

struct ClearInfo {
  std::vector<Color> colors;
  float depthClearValue = 1.0F;
  int stencilClearValue = 0;
  bool clearDepth = false;
  bool clearStencil = false;
};

auto Clear(const GraphicsContext &context, const ClearInfo &clearInfo) -> Error;

} // namespace Graphics::RenderState
