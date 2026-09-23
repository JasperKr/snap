#include "material.hpp"
#include "Modules/Math/vector.hpp"
#include "Modules/error.hpp"
#include "Wrap/Helpers/lua_enum.hpp"
#include "Wrap/wrap_engine.hpp"
#include "renderer.hpp"
#include <cstdint>
#include <imgui.h>
#include <lua.hpp>

namespace Engine::Renderer {

const static ::LuaWrap::LuaEnum<AlphaMode>
    LuaAlphaModeEnum("AlphaMode", {
                                      {"opaque", AlphaMode::Opaque},
                                      {"mask", AlphaMode::Mask},
                                      {"blend", AlphaMode::Blend},
                                  });

const static ::LuaWrap::LuaEnum<VkCullModeFlags>
    LuaCullModeEnum("CullMode", {
                                    {"none", VK_CULL_MODE_NONE},
                                    {"front", VK_CULL_MODE_FRONT_BIT},
                                    {"back", VK_CULL_MODE_BACK_BIT},
                                    {"all", VK_CULL_MODE_FRONT_AND_BACK},
                                });

auto LuaMaterial::wrap_setTextureUVIndex(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto textureType = luaL_checkinteger(state, 2);
  if (textureType < 0 ||
      textureType >= static_cast<int>(material->textureUVIndices.size())) {
    return luaL_error(state, "Invalid texture type index");
  }

  auto uvIndex = luaL_checkinteger(state, 3);
  if (uvIndex < 0 || uvIndex > UINT8_MAX) {
    return luaL_error(state, "UV index must be between 0 and UINT8_MAX");
  }

  material->textureUVIndices.at(textureType) = static_cast<uint8_t>(uvIndex);
  return 0;
}

auto LuaMaterial::wrap_getTextureUVIndex(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }
  auto textureType = luaL_checkinteger(state, 2);
  if (textureType < 0 ||
      textureType >= static_cast<int>(material->textureUVIndices.size())) {
    return luaL_error(state, "Invalid texture type index");
  }

  lua_pushinteger(state, material->textureUVIndices.at(textureType));

  return 1;
}

auto LuaMaterial::wrap_setCullMode(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaCullModeEnum.FromLua(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.error().message.c_str());
  }

  material->cullMode = result.value();
  return 0;
}

auto LuaMaterial::wrap_getCullMode(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaCullModeEnum.ToLua(state, material->cullMode);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.message.c_str());
  }

  return 1;
}

auto LuaMaterial::wrap_setAlphaMode(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaAlphaModeEnum.FromLua(state, 2);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.error().message.c_str());
  }

  material->alphaMode = result.value();
  return 0;
}

auto LuaMaterial::wrap_getAlphaMode(lua_State *state) -> int {
  auto *entity = ::LuaWrap::EntityFromLua(state, 1);
  if (entity == nullptr) {
    return luaL_error(state, "Invalid Entity");
  }

  auto *material = entity->try_get_mut<Material>();
  if (material == nullptr) {
    return luaL_error(state, "Entity does not have a Material component");
  }

  auto result = LuaAlphaModeEnum.ToLua(state, material->alphaMode);
  if (Error::IsError(result)) {
    return luaL_error(state, "%s", result.message.c_str());
  }

  return 1;
}

// NOLINTNEXTLINE
auto Material::DrawGUI(flecs::entity entity) -> void {
  // ImGui::Text("Cull Mode: %s", LuaCullModeEnum.ToString(cullMode).c_str());
  // ImGui::Text("Alpha Mode: %s", LuaAlphaModeEnum.ToString(alphaMode).c_str());

  if (ImGui::BeginCombo("Cull Mode",
                        LuaCullModeEnum.ToString(cullMode).c_str())) {
    for (const auto &option : LuaCullModeEnum.GetOptions()) {
      bool isSelected = option.second == cullMode;
      if (ImGui::Selectable(option.first.c_str(), isSelected)) {
        cullMode = option.second;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (ImGui::BeginCombo("Alpha Mode",
                        LuaAlphaModeEnum.ToString(alphaMode).c_str())) {
    for (const auto &option : LuaAlphaModeEnum.GetOptions()) {
      bool isSelected = option.second == alphaMode;
      if (ImGui::Selectable(option.first.c_str(), isSelected)) {
        alphaMode = option.second;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::ColorEdit4("Albedo Factor", albedoFactor.Ptr());
  ImGui::SliderFloat("Roughness Factor", &roughnessFactor, 0.0F, 1.0F);
  ImGui::SliderFloat("Metallic Factor", &metallicFactor, 0.0F, 1.0F);
  ImGui::SliderFloat("Reflectance Factor", &reflectanceFactor, 0.0F, 1.0F);
  ImGui::DragFloat3("Emissive Factor", emissiveFactor.Ptr(), 0.01F); // NOLINT
  ImGui::SliderFloat("Alpha Cutoff", &alphaCutoff, 0.0F, 1.0F);

  ImGui::Separator();

  if (ImGui::CollapsingHeader("Texture UV Indices")) {
    constexpr int MaxUVSets = 4;

    // NOLINTBEGIN (magic numbers)
    ImGui::SliderInt("Albedo", &textureUVIndices.at(0), 0, MaxUVSets);
    ImGui::SliderInt("Normal", &textureUVIndices.at(1), 0, MaxUVSets);
    ImGui::SliderInt("Roughness", &textureUVIndices.at(2), 0, MaxUVSets);
    ImGui::SliderInt("Metallic", &textureUVIndices.at(3), 0, MaxUVSets);
    ImGui::SliderInt("AO", &textureUVIndices.at(4), 0, MaxUVSets);
    ImGui::SliderInt("Reflectance", &textureUVIndices.at(5), 0, MaxUVSets);
    ImGui::SliderInt("Emissive", &textureUVIndices.at(6), 0, MaxUVSets);
    // NOLINTEND (magic numbers)
  }

  constexpr ImVec2 PreviewSize = ImVec2(200, 200);

  ImGui::BeginDisabled(!albedoTexture.isValid());
  if (ImGui::CollapsingHeader("Albedo Texture")) {
    if (albedoTexture.isValid()) {
      // NOLINTNEXTLINE
      ImGui::Image(reinterpret_cast<ImTextureID>(albedoTexture.get()),
                   PreviewSize);
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!metallicRoughnessTexture.isValid());
  if (ImGui::CollapsingHeader("Metallic Roughness Texture")) {
    if (metallicRoughnessTexture.isValid()) {
      ImGui::Image( // NOLINTNEXTLINE
          reinterpret_cast<ImTextureID>(metallicRoughnessTexture.get()),
          PreviewSize);
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!ambientOcclusionTexture.isValid());
  if (ImGui::CollapsingHeader("Ambient Occlusion Texture")) {
    if (ambientOcclusionTexture.isValid()) {
      ImGui::Image( // NOLINTNEXTLINE
          reinterpret_cast<ImTextureID>(ambientOcclusionTexture.get()),
          PreviewSize);
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!reflectanceTexture.isValid());
  if (ImGui::CollapsingHeader("Normal Texture")) {
    if (normalTexture.isValid()) {
      ImGui::Image( // NOLINTNEXTLINE
          reinterpret_cast<ImTextureID>(normalTexture.get()), PreviewSize);
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!reflectanceTexture.isValid());
  if (ImGui::CollapsingHeader("Emissive Texture")) {
    if (emissiveTexture.isValid()) {
      ImGui::Image( // NOLINTNEXTLINE
          reinterpret_cast<ImTextureID>(emissiveTexture.get()), PreviewSize);
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!preview.isValid());
  if (ImGui::CollapsingHeader("Reflectance Texture")) {
    if (reflectanceTexture.isValid()) {
      ImGui::Image( // NOLINTNEXTLINE
          reinterpret_cast<ImTextureID>(reflectanceTexture.get()), PreviewSize);
    }
  }
  ImGui::EndDisabled();

  dirty = true;
}

auto Material::Update(Graphics::GraphicsContext &context) -> Error {
  if (!dirty && obtainedSSBOIndex) {
    return {};
  }

  dirty = false;

  if (!obtainedSSBOIndex) {
    materialSSBOIndex = CHECK_RES(RendererInstance.GetNewMaterialIndex());
    obtainedSSBOIndex = true;
  }

  return Write(context);
}

auto Material::Write(Graphics::GraphicsContext &context) -> Error {

  // NOLINTBEGIN (hicppp-avoid-c-arrays)
  struct MaterialData {
    alignas(4) float albedoFactor[4];
    alignas(4) float roughnessFactor;
    alignas(4) float metallicFactor;
    alignas(4) float reflectanceFactor;
    alignas(4) uint32_t padding;
    alignas(4) float emissiveFactor[3];
    alignas(4) uint32_t alphaMode;
    alignas(4) float alphaCutoff;
    alignas(4) uint32_t textureUVIndices[2];
    alignas(4) uint32_t padding2;
  } materialData{};
  // NOLINTEND (hicppp-avoid-c-arrays)

  materialData.albedoFactor[0] = albedoFactor.x;
  materialData.albedoFactor[1] = albedoFactor.y;
  materialData.albedoFactor[2] = albedoFactor.z;
  materialData.albedoFactor[3] = albedoFactor.w;

  materialData.roughnessFactor = roughnessFactor;
  materialData.metallicFactor = metallicFactor;
  materialData.reflectanceFactor = reflectanceFactor;

  materialData.emissiveFactor[0] = emissiveFactor.x;
  materialData.emissiveFactor[1] = emissiveFactor.y;
  materialData.emissiveFactor[2] = emissiveFactor.z;

  materialData.alphaCutoff = alphaCutoff;
  materialData.alphaMode = static_cast<uint32_t>(alphaMode);

  uint64_t packedIndices = 0;
  for (size_t i = 0; i < textureUVIndices.size(); ++i) {
    packedIndices |=
        (static_cast<uint64_t>(textureUVIndices.at(i)) << (i * UINT8_WIDTH));
  }
  materialData.textureUVIndices[0] = packedIndices & ~0U;
  materialData.textureUVIndices[1] = (packedIndices >> UINT32_WIDTH) & ~0U;

  // auto span = std::span<MaterialData>(&materialData, 1);

  // return buffer->GetBuffer()->SetData(context, span,
  //                                     materialSSBOIndex * buffer->GetStride());

  auto *ptr =
      RendererInstance.GetMaterialUploadManager().GetPtrAt(materialSSBOIndex);
  memcpy(ptr, &materialData, sizeof(MaterialData));
  RendererInstance.GetMaterialUploadManager().MarkUpdated(materialSSBOIndex);

  return {};
}

} // namespace Engine::Renderer