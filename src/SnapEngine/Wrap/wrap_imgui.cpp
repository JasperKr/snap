#include "wrap_imgui.hpp"
#include "Editor/gui.hpp"
#include "Graphics/draw.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/graphicsState.hpp"
#include "Graphics/mesh.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/texture.hpp"
#include "Graphics/uniformWriter.hpp"
#include "Graphics/vertexformat.hpp"
#include "Modules/Peripherals/keyboard.hpp"
#include "Modules/Peripherals/mouse.hpp"
#include "Modules/error.hpp"
#include "Modules/imageData.hpp"
#include "Modules/object.hpp"
#include "Wrap/wrap.hpp"
#include "imgui.h"

#include "style.hpp"
#include "vulkan/vulkan_core.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

#include <lua.hpp>

namespace Wrap::Imgui {

auto NewFrame(lua_State *state) -> int {
  auto deltaTime = static_cast<float>(luaL_checknumber(state, 1));
  auto &inout = ImGui::GetIO();
  auto *ctx = Graphics::GetCurrentGraphicsContext();

  inout.DisplaySize.x = static_cast<float>(ctx->swapchainInfo.extent.width);
  inout.DisplaySize.y = static_cast<float>(ctx->swapchainInfo.extent.height);

  inout.DeltaTime = deltaTime;

  if (inout.WantSetMousePos) {
    Mouse::SetPosition(inout.MousePos.x, inout.MousePos.y);
  }

  ImGui::NewFrame();
  return 0;
}

auto EndFrame(lua_State *state) -> int {
  ImGui::EndFrame();
  return 0;
}

const Graphics::VertexFormat format{{
    Graphics::VertexComponent{.name = "Position",
                              .location = 0,
                              .binding = 0,
                              .format = VK_FORMAT_R32G32_SFLOAT},
    Graphics::VertexComponent{.name = "UV",
                              .location = 1,
                              .binding = 0,
                              .format = VK_FORMAT_R32G32_SFLOAT},
    Graphics::VertexComponent{.name = "Color",
                              .location = 2,
                              .binding = 0,
                              .format = VK_FORMAT_R8G8B8A8_UNORM},
}};

inline auto ChangeMouseState(ImGuiIO &inout) -> Error {
  if ((static_cast<uint32_t>(inout.ConfigFlags) &
       ImGuiConfigFlags_NoMouseCursorChange) == 0) {
    auto imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || inout.MouseDrawCursor) {
      Mouse::SetVisible(false);
    } else {
      Mouse::SetVisible(true);
      const auto &imgui_cursor_to_mouse_cursor =
          Engine::Gui::GetImGuiCursorMap();

      const auto &iterator = imgui_cursor_to_mouse_cursor.find(imgui_cursor);
      if (iterator != imgui_cursor_to_mouse_cursor.end()) {
        if (iterator->second->sdlCursor == nullptr) {
          return Error::Create("ImGui mouse cursor not created");
        }

        auto setResult = Mouse::SetCursor(iterator->second);
        if (Error::IsError(setResult)) {
          return setResult;
        }
      } else {
        return Error::Create("Unmapped ImGui mouse cursor type: " +
                             std::to_string(static_cast<int>(imgui_cursor)));
      }
    }
  }

  return Error::Success();
}

inline auto HandleImguiCreateTextureEvent(Graphics::GraphicsContext &context,
                                          ImTextureData *tex) -> Error {
  Graphics::TextureCreationInfo createInfo{
      .size = VkExtent3D{static_cast<uint32_t>(tex->Width),
                         static_cast<uint32_t>(tex->Height), 1},
      .format = tex->BytesPerPixel == 1 ? VK_FORMAT_R8_UNORM
                                        : Graphics::DefaultPixelFormat,
      .usage = static_cast<uint32_t>(VK_IMAGE_USAGE_SAMPLED_BIT) |
               static_cast<uint32_t>(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
      .mipmapCount = 1,
      .debugName = "Imgui Texture",
      .textureType = ::Graphics::TextureType::DEFAULT,
  };

  auto textureCreationResult = Graphics::Texture::Create(context, createInfo);

  if (Error::IsError(textureCreationResult)) {
    return textureCreationResult.error();
  }

  auto texture = textureCreationResult.value();

  tex->SetTexID( // NOLINTNEXTLINE reinterpret-cast
      reinterpret_cast<ImTextureID>(texture.get()));

  texture->SetFilter(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                     VK_SAMPLER_MIPMAP_MODE_LINEAR);

  Engine::Gui::ImGuiTextures.emplace(tex->GetTexID(), texture);

  auto pixelSpan = std::span<uint8_t>(
      tex->Pixels,
      static_cast<size_t>(tex->Width * tex->Height * tex->BytesPerPixel));

  auto imagedataResult = Image::ImageData::Create(
      createInfo.size,
      tex->BytesPerPixel == 1 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8B8A8_UNORM);

  if (Error::IsError(imagedataResult)) {
    return imagedataResult.error();
  }

  auto imagedata = imagedataResult.value();
  std::memcpy(imagedata->GetDataPtr(), pixelSpan.data(),
              pixelSpan.size_bytes());

  auto setPixelsResult = texture->SetPixels(context, *imagedata);

  if (Error::IsError(setPixelsResult)) {
    return setPixelsResult;
  }

  tex->SetStatus(ImTextureStatus_OK);

  return Error::Success();
}

inline auto HandleImguiDestroyTextureEvent(ImTextureData *tex) -> Error {
  auto *texture = // NOLINTNEXTLINE reinterpret-cast
      reinterpret_cast<Graphics::Texture *>(tex->GetTexID());

  if (texture == nullptr) {
    return Error::Create("Attempted to destroy null ImGui texture.");
  }

  auto iter = Engine::Gui::ImGuiTextures.find(tex->GetTexID());
  if (iter != Engine::Gui::ImGuiTextures.end()) {
    Engine::Gui::ImGuiTextures.erase(iter);
  }

  tex->SetTexID(0);
  tex->SetStatus(ImTextureStatus_Destroyed);

  return Error::Success();
}

inline auto HandleImguiUpdateTextureEvent(Graphics::GraphicsContext &context,
                                          ImTextureData *tex) -> Error {
  auto *pixels = static_cast<uint8_t *>(tex->GetPixels());
  auto pixelSpan =
      std::span<uint8_t>(pixels, static_cast<size_t>(tex->Width * tex->Height *
                                                     tex->BytesPerPixel));

  auto *texture = // NOLINTNEXTLINE reinterpret-cast
      reinterpret_cast<Graphics::Texture *>(tex->GetTexID());

  std::lock_guard<std::mutex> lock(texture->mutex);

  for (ImTextureRect &currentRect : tex->Updates) {
    VkExtent3D destSize{
        .width = static_cast<uint32_t>(currentRect.w),
        .height = static_cast<uint32_t>(currentRect.h),
        .depth = 1,
    };

    VkOffset3D destOffset{
        .x = static_cast<int32_t>(currentRect.x),
        .y = static_cast<int32_t>(currentRect.y),
        .z = 0,
    };

    VkOffset3D sourceOffset = destOffset;

    // Use the update data from the current pixels
    auto updateResult =
        texture->SetPixels(context, pixelSpan, 0, 0,
                           {static_cast<uint32_t>(tex->Width),
                            static_cast<uint32_t>(tex->Height), 1},
                           sourceOffset, destOffset, destSize);

    if (Error::IsError(updateResult)) {
      return updateResult;
    }
  }

  tex->SetStatus(ImTextureStatus_OK);

  return Error::Success();
}

std::vector<TemporaryCommandList> TemporaryCommandLists; // NOLINT

inline auto SetupTemporaryCommandLists(ImDrawData *drawData,
                                       Graphics::GraphicsContext &ctx)
    -> Error {

  TemporaryCommandLists.reserve(drawData->CmdListsCount);
  if (TemporaryCommandLists.size() < drawData->CmdListsCount) {
    TemporaryCommandLists.resize(drawData->CmdListsCount);
  }

  for (int i = 0; i < drawData->CmdListsCount; ++i) {
    auto *commandList = drawData->CmdLists[i];

    auto &temporaryCommandList = TemporaryCommandLists[i];
    temporaryCommandList.DrawList = commandList;

    auto vertexCount = commandList->VtxBuffer.Size;
    auto indexCount = commandList->IdxBuffer.Size;

    if (vertexCount == 0 || indexCount == 0) {
      continue;
    }

    if (vertexCount > temporaryCommandList.MaxVertexCount) {
      temporaryCommandList.MaxVertexCount = vertexCount;

      Graphics::MeshCreationInfo info{
          .vertexFormat = &format,
          .vertexCount = static_cast<uint64_t>(vertexCount),
          .debugName = "Imgui UI Mesh",
      };

      temporaryCommandList.Mesh = CHECK_RES(Graphics::Mesh::Create(ctx, info));
    }

    auto vertexSpan =
        std::span<ImDrawVert>(commandList->VtxBuffer.Data,
                              static_cast<size_t>(commandList->VtxBuffer.Size));

    auto rawVertexSpan = std::span<uint8_t>(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        reinterpret_cast<uint8_t *>(vertexSpan.data()),
        vertexSpan.size_bytes());

    CHECK_ERR(temporaryCommandList.Mesh->SetVertices(ctx, 0, rawVertexSpan, 0));

    auto indexSpan = std::span<uint8_t>( // NOLINTNEXTLINE
        reinterpret_cast<uint8_t *>(commandList->IdxBuffer.Data),
        static_cast<size_t>(commandList->IdxBuffer.size() * 2));

    CHECK_ERR(temporaryCommandList.Mesh->SetIndices(ctx, indexSpan,
                                                    VK_INDEX_TYPE_UINT16));
  }

  return Error::Success();
}

inline auto DrawTemporaryCommandLists(Graphics::GraphicsContext &ctx,
                                      ImDrawData *drawData) -> Error {

  Graphics::RenderState::SetShader(Engine::Gui::ImGuiShaderRGBA8);
  const auto &viewport = Graphics::RenderState::GetClippedViewport();

  static auto projectionMatrixKey =
      Graphics::ResourceKey{"PushConstants", "ProjectionMatrix"};

  auto translationMatrix = Math::Matrix4x4::TranslationMatrix(
      {-viewport.width / 2.0F,          // NOLINT
       -viewport.height / 2.0F, 0.0F}); // NOLINT

  Math::Matrix4x4 projectionMatrix = Math::Matrix4x4::Orthographic(
      viewport.width, viewport.height, 0.0F, 1.0F);

  auto viewProjectionMatrix = translationMatrix * projectionMatrix;

  for (int i = 0; drawData->CmdListsCount > i; ++i) {
    const auto &temporaryCommandList = TemporaryCommandLists[i];
    auto *commandList = temporaryCommandList.DrawList;

    for (int cmd_i = 0; commandList->CmdBuffer.Size > cmd_i; ++cmd_i) {
      const auto &pcmd = commandList->CmdBuffer[cmd_i];

      if (pcmd.UserCallback != nullptr) {
        pcmd.UserCallback(commandList, &pcmd);
      } else {
        VkRect2D scissorRect{
            .offset{
                .x = static_cast<int32_t>(pcmd.ClipRect.x),
                .y = static_cast<int32_t>(pcmd.ClipRect.y),
            },
            .extent{
                .width =
                    static_cast<uint32_t>(pcmd.ClipRect.z - pcmd.ClipRect.x),
                .height =
                    static_cast<uint32_t>(pcmd.ClipRect.w - pcmd.ClipRect.y),
            },
        };
        Graphics::RenderState::SetScissor(&scissorRect);

        auto *texture = // NOLINTNEXTLINE
            CHECK_NULL(reinterpret_cast<Graphics::Texture *>(pcmd.GetTexID()));

        auto texRef = Ref<Graphics::Texture>(texture);
        CHECK_ERR(Engine::Gui::ImGuiShaderRGBA8->Send({"MainTexture"}, texRef));

        CHECK_ERR(Graphics::UniformWriter::Send(Engine::Gui::ImGuiShaderRGBA8,
                                                projectionMatrixKey,
                                                viewProjectionMatrix));

        temporaryCommandList.Mesh->SetDrawRange({
            .Offset = static_cast<uint32_t>(pcmd.IdxOffset),
            .Count = static_cast<uint32_t>(pcmd.ElemCount),
        });

        CHECK_ERR(Graphics::Draw(ctx, *temporaryCommandList.Mesh));
      }
    }
  }

  return Error::Success();
}

auto Draw(lua_State *state) -> int {
  ImGui::Render();

  auto ctx = *Graphics::GetCurrentGraphicsContext();

  auto inout = ImGui::GetIO();
  Graphics::RenderState::SetCullMode(VK_CULL_MODE_NONE);
  Graphics::RenderState::SetDepthMode(false, false, VK_COMPARE_OP_ALWAYS);

  LUA_CK_ERR(ChangeMouseState(inout));
  auto *drawData = LUA_CK_NULL(ImGui::GetDrawData());

  for (ImTextureData *tex : *drawData->Textures) {
    if (tex->Status == ImTextureStatus_WantCreate) {
      LUA_CK_ERR(HandleImguiCreateTextureEvent(ctx, tex));
    } else if (tex->Status == ImTextureStatus_WantDestroy) {
      LUA_CK_ERR(HandleImguiDestroyTextureEvent(tex));
    } else if (tex->Status == ImTextureStatus_WantUpdates) {
      LUA_CK_ERR(HandleImguiUpdateTextureEvent(ctx, tex));
    }
  }

  LUA_CK_ERR(SetupTemporaryCommandLists(drawData, ctx));
  LUA_CK_ERR(DrawTemporaryCommandLists(ctx, drawData));

  return 0;
}

const std::array<int, 3> ImGuiMouseButtonRemap{
    0, // 0 -> 0
    2, // 1 -> 2
    1  // 2 -> 1
};

// Imgui event passthrough functions
auto MousePressed(lua_State *state) -> int {
  // x, y, button: 1, 2, 3
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  auto button = static_cast<int>(luaL_checkinteger(state, 3)) - 1; // 0-based

  auto &inout = ImGui::GetIO();

  // NOLINTBEGIN
  if (button >= 0 && button < IM_ARRAYSIZE(inout.MouseDown)) {
    inout.AddMouseButtonEvent(ImGuiMouseButtonRemap.at(button), true);
  }
  // NOLINTEND

  return 0;
}

auto MouseReleased(lua_State *state) -> int {
  // x, y, button: 1, 2, 3
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));
  auto button = static_cast<int>(luaL_checkinteger(state, 3)) - 1; // 0-based

  auto &inout = ImGui::GetIO();
  // NOLINTBEGIN
  if (button >= 0 && button < IM_ARRAYSIZE(inout.MouseDown)) {
    inout.AddMouseButtonEvent(ImGuiMouseButtonRemap.at(button), false);
  }
  // NOLINTEND

  return 0;
}

auto MouseMoved(lua_State *state) -> int {
  // x, y
  auto x_position = static_cast<float>(luaL_checknumber(state, 1));
  auto y_position = static_cast<float>(luaL_checknumber(state, 2));

  auto &inout = ImGui::GetIO();
  inout.AddMousePosEvent(x_position, y_position);

  return 0;
}

auto KeyPressed(lua_State *state) -> int {
  // key is a, b, c, lshift, rshift, ctrl, alt, etc.
  const auto *key = luaL_checkstring(state, 1);

  // use keyboard module to get SDL keycode
  auto keycodeMap = Keyboard::StringToKeycodeMap;
  auto iterator = keycodeMap.find(key);
  if (iterator == keycodeMap.end()) {
    return luaL_error(state,
                      "Unknown key string passed to ImGui KeyPressed: %s", key);
  }

  auto keycode = static_cast<SDL_Keycode>(iterator->second);

  auto scanecodeMap = Keyboard::StringToScancodeMap;
  auto scancodeIterator = scanecodeMap.find(key);
  if (scancodeIterator == scanecodeMap.end()) {
    return luaL_error(state,
                      "Unknown key string passed to ImGui KeyPressed: %s", key);
  }

  auto scancode = static_cast<SDL_Scancode>(scancodeIterator->second);

  auto &inout = ImGui::GetIO();
  auto imkey = Engine::Gui::KeyEventToImguiKey(keycode, scancode);

  inout.AddKeyEvent(imkey, true);

  return 0;
}
auto KeyReleased(lua_State *state) -> int {
  // key is a, b, c, lshift, rshift, ctrl, alt, etc.
  const auto *key = luaL_checkstring(state, 1);

  // use keyboard module to get SDL keycode
  auto keycodeMap = Keyboard::StringToKeycodeMap;
  auto iterator = keycodeMap.find(key);
  if (iterator == keycodeMap.end()) {
    return luaL_error(
        state, "Unknown key string passed to ImGui KeyReleased: %s", key);
  }

  auto keycode = static_cast<SDL_Keycode>(iterator->second);

  auto scanecodeMap = Keyboard::StringToScancodeMap;
  auto scancodeIterator = scanecodeMap.find(key);
  if (scancodeIterator == scanecodeMap.end()) {
    return luaL_error(
        state, "Unknown key string passed to ImGui KeyReleased: %s", key);
  }

  auto scancode = static_cast<SDL_Scancode>(scancodeIterator->second);

  auto &inout = ImGui::GetIO();
  auto imkey = Engine::Gui::KeyEventToImguiKey(keycode, scancode);

  inout.AddKeyEvent(imkey, false);

  return 0;
}

auto TextInput(lua_State *state) -> int {
  // text
  const auto *text = luaL_checkstring(state, 1);

  auto &inout = ImGui::GetIO();
  inout.AddInputCharactersUTF8(text);

  return 0;
}

auto MouseWheelMoved(lua_State *state) -> int {
  // x, y
  auto x_scroll = static_cast<float>(luaL_checknumber(state, 1));
  auto y_scroll = static_cast<float>(luaL_checknumber(state, 2));

  auto &inout = ImGui::GetIO();
  inout.AddMouseWheelEvent(x_scroll, y_scroll);

  return 0;
}

auto Shutdown(lua_State *state) -> int {
  auto shutdownResult = Engine::Gui::ShutdownImGui();
  if (Error::IsError(shutdownResult)) {
    return luaL_error(state, "Failed to shutdown ImGui: %s",
                      shutdownResult.message.c_str());
  }

  TemporaryCommandLists.clear();

  return 0;
}

auto GetImguiContextPtr(lua_State *state) -> int {
  // bitcast the ImGui context pointer to a double (since Lua doesn't have a native pointer type) and return it
  auto *context = ImGui::GetCurrentContext();
  auto contextValue = reinterpret_cast<uintptr_t>(context); // NOLINT
  lua_pushnumber(state, static_cast<lua_Number>(contextValue));
  return 1;
}

auto GetImguiFontAtlasPtr(lua_State *state) -> int {
  // bitcast the ImGui font atlas pointer to a double (since Lua doesn't have a native pointer type) and return it
  auto *fontAtlas = ImGui::GetIO().Fonts;
  auto fontAtlasValue = reinterpret_cast<uintptr_t>(fontAtlas); // NOLINT
  lua_pushnumber(state, static_cast<lua_Number>(fontAtlasValue));
  return 1;
}

inline auto stringToUIStyle(const char *styleStr)
    -> Result<Engine::Style::UIStyles> {
  if (strcmp(styleStr, "blueish") == 0) {
    return Engine::Style::UIStyles::Blueish;
  }
  if (strcmp(styleStr, "greenish") == 0) {
    return Engine::Style::UIStyles::Greenish;
  }
  return Error::Unexpectedf("Unknown UI style: " + std::string(styleStr));
}

// auto ApplyDefaultStyle(UIStyles uiStyle) -> Error;
// auto wrap_ApplyDefaultStyle(lua_State *state) -> int;

auto wrap_ApplyDefaultStyle(lua_State *state) -> int {
  const char *styleStr = luaL_optstring(state, 1, "blueish");
  auto styleResult = stringToUIStyle(styleStr);
  if (Error::IsError(styleResult)) {
    return luaL_error(state, "Failed to apply default style: %s",
                      styleResult.error().message.c_str());
  }

  auto applyResult = ApplyDefaultStyle(styleResult.value());
  if (Error::IsError(applyResult)) {
    return luaL_error(state, "Failed to apply default style: %s",
                      applyResult.message.c_str());
  }

  return 0;
}

} // namespace Wrap::Imgui