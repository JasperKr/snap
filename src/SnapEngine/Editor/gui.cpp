#include "gui.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/object.hpp"
#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL_mouse.h"
#include "imgui.h"
#include "imstb_truetype.h"
#include "style.hpp"
#include <array>
#include <cassert>
#include <unordered_map>

namespace Engine::Gui {

// NOLINTBEGIN
Ref<Graphics::Shader> ImGuiShaderRGBA8;
Ref<Graphics::Shader> ImGuiShaderA8;
std::vector<std::vector<unsigned char>> ImGuiFonts{};
// NOLINTEND

auto MainWindow() -> void {}

auto LoadGUIState(lua_State *state) -> Error {
  CHECK_ERR(LoadImGuiCursorMap());

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui::StyleColorsDark();

  auto &inout = ImGui::GetIO();
  auto flags = static_cast<uint32_t>(inout.ConfigFlags);
  flags |= static_cast<uint32_t>(ImGuiConfigFlags_NavEnableKeyboard); // Enable Keyboard Controls
  inout.ConfigFlags = static_cast<ImGuiConfigFlags>(flags);

  auto &platformIO = ImGui::GetPlatformIO();
  platformIO.Platform_GetClipboardTextFn = [](ImGuiContext *context) -> const char * { return SDL_GetClipboardText(); };
  platformIO.Platform_SetClipboardTextFn = [](ImGuiContext *context, const char *text) -> void { SDL_SetClipboardText(text); };

  float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  assert(scale > 0.0F && "Display content scale must be greater than 0");

  ImGuiStyle &style = ImGui::GetStyle();
  auto error = Engine::Style::ApplyDefaultStyle(Engine::Style::UIStyles::Greenish);
  if (Error::IsError(error)) {
    return error;
  }

  style.FontScaleDpi = scale;
  style.ScaleAllSizes(scale);

  const float baseFontSize = 18.0F;

  ImFontConfig config;
  config.OversampleH = 5; // NOLINT
  config.OversampleV = 5; // NOLINT
  config.PixelSnapH = true;
  config.SizePixels = baseFontSize * scale;
  config.FontDataOwnedByAtlas = false;

  ImGui::GetIO().Fonts->AddFontDefault(&config);

  const auto &sourceDirectory = Filesystem::GetSourceDirectory();
  const std::string &fontPath = "Graphics/Assets/user_interface_font.ttf";
  const auto &fontData = CHECK_RES(Filesystem::ReadFile(fontPath));

  ImGuiFonts.emplace_back(fontData);

  ImFont *font = inout.Fonts->AddFontFromMemoryTTF((void *)ImGuiFonts.back().data(), (int)ImGuiFonts.back().size(), baseFontSize, &config);

  // Set the font as the default font
  inout.FontDefault = font;

  flags = static_cast<uint32_t>(inout.BackendFlags);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_RendererHasTextures);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_HasSetMousePos);
  flags |= static_cast<uint32_t>(ImGuiBackendFlags_HasMouseCursors);
  inout.BackendFlags = static_cast<ImGuiBackendFlags>(flags);

  auto ctx = *Graphics::GetCurrentGraphicsContext();

  ImGuiShaderRGBA8 = CHECK_RES(Graphics::Shader::Create(ctx, "Scripting/Graphics/Shaders/GUI/ImGuiRGBA8", "Imgui rgba8 shader"));

  ImGuiShaderA8 = CHECK_RES(Graphics::Shader::Create(ctx, "Scripting/Graphics/Shaders/GUI/ImGuiA8", "Imgui a8 shader"));

  return {};
}

auto LoadImGuiCursorMap() -> Error {
  auto &map = GetImGuiCursorMap();

  struct CursorMapping {
    int imguiCursor;
    SDL_SystemCursor sdlCursor;
  };

  static const std::array<CursorMapping, 11> mappings{
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Arrow, .sdlCursor = SDL_SYSTEM_CURSOR_DEFAULT},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_TextInput, .sdlCursor = SDL_SYSTEM_CURSOR_TEXT},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeAll, .sdlCursor = SDL_SYSTEM_CURSOR_MOVE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeNS, .sdlCursor = SDL_SYSTEM_CURSOR_NS_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeEW, .sdlCursor = SDL_SYSTEM_CURSOR_EW_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeNESW, .sdlCursor = SDL_SYSTEM_CURSOR_NESW_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_ResizeNWSE, .sdlCursor = SDL_SYSTEM_CURSOR_NWSE_RESIZE},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Hand, .sdlCursor = SDL_SYSTEM_CURSOR_POINTER},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Wait, .sdlCursor = SDL_SYSTEM_CURSOR_WAIT},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_Progress, .sdlCursor = SDL_SYSTEM_CURSOR_PROGRESS},
      CursorMapping{.imguiCursor = ImGuiMouseCursor_NotAllowed, .sdlCursor = SDL_SYSTEM_CURSOR_NOT_ALLOWED},
  };

  for (const auto &mapping : mappings) {
    map[mapping.imguiCursor] = CHECK_RES(Mouse::CreateSystemCursor(mapping.sdlCursor));
  }

  return Error::Success();
}

std::unordered_map<ImTextureID, Ref<Graphics::Texture>> ImGuiTextures{}; // NOLINT

auto ShutdownImGui() -> Error {
  ImGui::DestroyContext();
  ImGuiTextures.clear();

  ImGuiShaderRGBA8.reset(); // Release reference
  ImGuiShaderA8.reset();

  return Error::Success();
}

} // namespace Engine::Gui