#include "style.hpp"
#include <imgui.h>

namespace Engine::Style {

inline auto ToImVec4(const Color &color) -> ImVec4 {
  auto corrected = color.ToLinear();
  return {corrected.r, corrected.g, corrected.b, corrected.a};
}

inline auto ToImVec4(const Color &color, float alpha) -> ImVec4 {
  auto corrected = color.ToLinear();
  return {corrected.r, corrected.g, corrected.b, alpha};
}

auto ApplyDefaultStyle(UIStyles uiStyle) -> Error {
  auto &style = ImGui::GetStyle();

  constexpr auto Rounding = 5.0F;
  constexpr auto BorderSize = 1.0F;

  // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
  style.Alpha = 1.0F;
  style.DisabledAlpha = 0.6F;
  style.WindowPadding = ImVec2(8.0F, 8.0F);
  style.WindowRounding = Rounding;
  style.WindowBorderSize = BorderSize;
  style.WindowMinSize = ImVec2(32.0F, 32.0F);
  style.ChildRounding = Rounding;
  style.ChildBorderSize = BorderSize;
  style.PopupRounding = Rounding;
  style.PopupBorderSize = BorderSize;
  style.FramePadding = ImVec2(8.0F, 4.0F);
  style.FrameRounding = Rounding;
  style.FrameBorderSize = BorderSize;
  style.ItemSpacing = ImVec2(8.0F, 4.0F);
  style.ItemInnerSpacing = ImVec2(4.0F, 4.0F);
  style.CellPadding = ImVec2(4.0F, 2.0F);
  style.TouchExtraPadding = ImVec2(0.0F, 0.0F);
  style.IndentSpacing = 21.0F;
  style.ColumnsMinSpacing = 6.0F;
  style.ScrollbarSize = 14.0F;
  style.ScrollbarRounding = 9.0F;
  style.GrabMinSize = 10.0F;
  style.GrabRounding = Rounding;
  style.TabRounding = Rounding;
  style.TabBorderSize = BorderSize;
  style.ColorButtonPosition = ImGuiDir_Right;
  style.WindowMenuButtonPosition = ImGuiDir_Left;
  style.ButtonTextAlign = ImVec2(0.5F, 0.5F);
  style.SelectableTextAlign = ImVec2(0.0F, 0.0F);
  style.DisplayWindowPadding = ImVec2(22.0F, 22.0F);
  style.DisplaySafeAreaPadding = ImVec2(4.0F, 4.0F);
  style.MouseCursorScale = 1.0F;
  style.AntiAliasedLines = true;
  style.AntiAliasedLinesUseTex = true;
  style.AntiAliasedFill = true;
  style.CurveTessellationTol = 1.25F;

  const auto &preset = UIStylePresets.find(uiStyle);
  if (preset == UIStylePresets.end()) {
    return Error::Create("Unknown UI style preset");
  }

  auto &colors = style.Colors;
  const auto &definition = preset->second;
  auto textColor = GetTextColorForBackground(definition.Primary);

  constexpr auto DisabledColorMultiplier = Color(0.6F, 0.6F, 0.6F, 1.0F);

  const auto &primary = definition.Primary;
  const auto &secondary = definition.Secondary;
  const auto &tertiary = definition.Tertiary;
  const auto &quaternary = definition.Quaternary;

  // Main text and backgrounds
  colors[ImGuiCol_Text] = ToImVec4(textColor);
  colors[ImGuiCol_TextDisabled] = ToImVec4(textColor * DisabledColorMultiplier);
  colors[ImGuiCol_WindowBg] = ToImVec4(primary);
  colors[ImGuiCol_ChildBg] = ToImVec4(Accent(primary, 0.8F));
  colors[ImGuiCol_PopupBg] = ToImVec4(Accent(primary, 0.9F));

  // Borders and shadows
  colors[ImGuiCol_Border] = ToImVec4(Accent(primary, 0.9F), 1.0F);
  colors[ImGuiCol_BorderShadow] = ToImVec4(Accent(primary, 0.8F), 1.0F);

  // Frames
  colors[ImGuiCol_FrameBg] = ToImVec4(Accent(primary), 1.F);
  colors[ImGuiCol_FrameBgHovered] = ToImVec4(Accent(primary, 1.05F), 1.F);
  colors[ImGuiCol_FrameBgActive] = ToImVec4(Accent(primary, 1.1F), 1.F);

  // Titles
  colors[ImGuiCol_TitleBg] = ToImVec4(primary);
  colors[ImGuiCol_TitleBgActive] = ToImVec4(Accent(primary, 1.1F));
  colors[ImGuiCol_TitleBgCollapsed] = ToImVec4(Accent(primary, 1.2F));

  // Menu bar
  colors[ImGuiCol_MenuBarBg] = ToImVec4(Accent(primary, 1.2F));

  // Scrollbars
  colors[ImGuiCol_ScrollbarBg] = ToImVec4(Accent(primary, 0.9F));
  colors[ImGuiCol_ScrollbarGrab] = ToImVec4(Accent(primary, 1.3F));
  colors[ImGuiCol_ScrollbarGrabHovered] = ToImVec4(Accent(primary, 1.5));
  colors[ImGuiCol_ScrollbarGrabActive] = ToImVec4(Accent(primary, 1.7F));

  // Check marks and sliders
  colors[ImGuiCol_CheckMark] = ToImVec4(secondary);
  colors[ImGuiCol_SliderGrab] = ToImVec4(Accent(secondary));
  colors[ImGuiCol_SliderGrabActive] = ToImVec4(Accent(secondary, 1.5F));

  // Buttons
  colors[ImGuiCol_Button] = ToImVec4(tertiary);
  colors[ImGuiCol_ButtonHovered] = ToImVec4(Accent(tertiary));
  colors[ImGuiCol_ButtonActive] = ToImVec4(Accent(tertiary, 1.5F));

  // Headers
  colors[ImGuiCol_Header] = ToImVec4(secondary);
  colors[ImGuiCol_HeaderHovered] = ToImVec4(Accent(secondary));
  colors[ImGuiCol_HeaderActive] = ToImVec4(Accent(secondary, 1.5F));

  // Separators
  colors[ImGuiCol_Separator] = ToImVec4(Accent(primary, 1.1F));
  colors[ImGuiCol_SeparatorHovered] = ToImVec4(Accent(primary, 1.3F));
  colors[ImGuiCol_SeparatorActive] = ToImVec4(Accent(primary, 1.5F));

  // Resize grips
  colors[ImGuiCol_ResizeGrip] = ToImVec4(Accent(secondary));
  colors[ImGuiCol_ResizeGripHovered] = ToImVec4(Accent(secondary, 2.0F));
  colors[ImGuiCol_ResizeGripActive] = ToImVec4(Accent(secondary, 2.5F));

  // Tabs
  colors[ImGuiCol_Tab] = ToImVec4(tertiary);
  colors[ImGuiCol_TabHovered] = ToImVec4(Accent(tertiary, 1.05));
  colors[ImGuiCol_TabSelected] = ToImVec4(Accent(tertiary, 1.1F));
  colors[ImGuiCol_TabSelectedOverline] = ToImVec4(Accent(tertiary, 1.15F));
  colors[ImGuiCol_TabDimmed] = ToImVec4(Accent(tertiary, 0.8F));
  colors[ImGuiCol_TabDimmedSelected] = ToImVec4(Accent(tertiary, 0.85F));
  colors[ImGuiCol_TabDimmedSelectedOverline] = ToImVec4(Accent(tertiary, 0.9F));

  // Docking
  colors[ImGuiCol_DockingPreview] = ToImVec4(Accent(primary));
  colors[ImGuiCol_DockingEmptyBg] = ToImVec4(Accent(primary, 2.0F));

  // Plots
  colors[ImGuiCol_PlotLines] = ToImVec4(tertiary);
  colors[ImGuiCol_PlotLinesHovered] = ToImVec4(Accent(tertiary, 1.0F));
  colors[ImGuiCol_PlotHistogram] = ToImVec4(tertiary);
  colors[ImGuiCol_PlotHistogramHovered] = ToImVec4(Accent(tertiary));

  // Tables
  colors[ImGuiCol_TableHeaderBg] = ToImVec4(primary);
  colors[ImGuiCol_TableBorderStrong] = ToImVec4(Accent(primary));
  colors[ImGuiCol_TableBorderLight] = ToImVec4(Accent(primary, 1.1F));
  colors[ImGuiCol_TableRowBg] = ToImVec4(Accent(primary, 1.2F));
  colors[ImGuiCol_TableRowBgAlt] = ToImVec4(Accent(primary, 1.3F));

  // Misc
  colors[ImGuiCol_TextLink] = ToImVec4(Accent(secondary));
  colors[ImGuiCol_TextSelectedBg] = ToImVec4(Accent(secondary, 1.5F));
  colors[ImGuiCol_TreeLines] = ToImVec4(Accent(primary, 2.0F));
  colors[ImGuiCol_DragDropTarget] = ToImVec4(Accent(primary));
  colors[ImGuiCol_NavCursor] = ToImVec4(Accent(primary));
  colors[ImGuiCol_NavWindowingHighlight] = ToImVec4(Accent(primary, 1.5F));
  colors[ImGuiCol_NavWindowingDimBg] = ToImVec4(Accent(primary));
  colors[ImGuiCol_ModalWindowDimBg] = ToImVec4(Accent(primary));

  // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

  return {};
};

auto LuminanceFromColor(const Color &color) -> float {
  // Perceived brightness calculation using the Rec. 709 formula NOLINTNEXTLINE
  return (color.r * 0.2126F) + (color.g * 0.7152F) + (color.b * 0.0722F);
};

auto ColorToBrightness(const Color &color) -> Brightness {
  float brightnessValue = LuminanceFromColor(color);

  // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
  if (brightnessValue >= 0.8F) {
    return Brightness::Bright;
  }
  if (brightnessValue >= 0.6F) {
    return Brightness::LightGray;
  }
  if (brightnessValue >= 0.4F) {
    return Brightness::DarkGray;
  }
  return Brightness::Dark;

  // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
}

auto GetTextColorForBackground(const Color &background) -> Color {
  auto brightness = ColorToBrightness(background);

  // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
  switch (brightness) {
  case Brightness::Bright:
    return {0x000000FF}; // Black text for bright backgrounds
  case Brightness::LightGray:
    return {0x202020FF}; // Dark gray text for light gray backgrounds
  case Brightness::DarkGray:
    return {0xCECECEFF}; // White text for dark gray backgrounds
  case Brightness::Dark:
    return {0xDDDDDDFF}; // White text for dark backgrounds
  default:
    return {0x000000FF}; // Fallback to black text
  }
  // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)
}

auto Accent(const Color &color, float accentStrength) -> Color {
  auto brightness = ColorToBrightness(color);
  // Bright colors get a darker accent, dark colors get a brighter accent
  // NOLINTBEGIN(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

  Color accentColor;

  switch (brightness) {
  case Brightness::Bright:
    accentColor =
        Desaturate(color, 0.2F * accentStrength) * 0.8F * accentStrength;
    break;
  case Brightness::LightGray:
    accentColor =
        Desaturate(color, 0.1F * accentStrength) * 0.9F * accentStrength;
    break;
  case Brightness::DarkGray:
    accentColor =
        Saturate(color, 0.1F * accentStrength) * 1.1F * accentStrength;
    break;
  case Brightness::Dark:
    accentColor =
        Saturate(color, 0.2F * accentStrength) * 1.2F * accentStrength;
    break;
  default:
    accentColor = color; // Fallback to original color
    break;
  }
  // NOLINTEND(readability-magic-numbers, cppcoreguidelines-avoid-magic-numbers)

  accentColor.a = color.a; // Preserve original alpha

  return accentColor;
}

auto Lerp(const Color &first, const Color &second, float time) -> Color {
  return Color{
      (first.r * (1.0F - time)) + (second.r * time),
      (first.g * (1.0F - time)) + (second.g * time),
      (first.b * (1.0F - time)) + (second.b * time),
      (first.a * (1.0F - time)) + (second.a * time),
  };
}

auto Saturate(const Color &color, float saturationFactor) -> Color {
  // Convert to grayscale using the perceived brightness
  float gray = LuminanceFromColor(color);

  // Interpolate between the original color and the grayscale color based on the saturation factor
  return Lerp(color, {gray, gray, gray, color.a}, 1.0F - saturationFactor);
}
auto Desaturate(const Color &color, float desaturationFactor) -> Color {
  // Convert to grayscale using the perceived brightness
  float gray = LuminanceFromColor(color);

  // Interpolate between the original color and the grayscale color based on the desaturation factor
  return Lerp(color, {gray, gray, gray, color.a}, desaturationFactor);
}

} // namespace Engine::Style