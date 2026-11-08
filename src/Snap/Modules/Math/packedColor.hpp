#include "Modules/Math/vector.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>
namespace Math {

struct PackedColor {
  static constexpr uint8_t MaxValue = 255;
  static constexpr uint8_t MinValue = 0;

  static constexpr auto White() -> PackedColor { return PackedColor(MaxValue, MaxValue, MaxValue, MaxValue); }

  static constexpr auto Black() -> PackedColor { return PackedColor(MaxValue, MinValue, MinValue, MinValue); }

  static constexpr auto Transparent() -> PackedColor { return PackedColor(MinValue, MinValue, MinValue, MinValue); }

  // RGBA color representation
  uint8_t r{};
  uint8_t g{};
  uint8_t b{};
  uint8_t a{};
  // ^ These are stored in abgr order to match the Vulkan format VK_FORMAT_R8G8B8A8_UNORM
  // NOLINTBEGIN

  PackedColor(std::initializer_list<uint8_t> list) {
    assert(list.size() == 4);
    r = *(list.begin() + 0);
    g = *(list.begin() + 1);
    b = *(list.begin() + 2);
    a = *(list.begin() + 3);
  }

  PackedColor(std::initializer_list<float> list) : PackedColor(list.begin()[0], list.begin()[1], list.begin()[2], list.begin()[3]) {}

  constexpr PackedColor() = default;

  constexpr PackedColor(float r_val, float g_val, float b_val, float a_val = 1.0F)
      : a(static_cast<uint8_t>(a_val * 255.0F)), b(static_cast<uint8_t>(b_val * 255.0F)), g(static_cast<uint8_t>(g_val * 255.0F)),
        r(static_cast<uint8_t>(r_val * 255.0F)) {}

  constexpr explicit PackedColor(uint8_t r_val, uint8_t g_val, uint8_t b_val, uint8_t a_val = MaxValue) : a(a_val), b(b_val), g(g_val), r(r_val) {}

  constexpr explicit PackedColor(const Vec4 &vec4)
      : a(static_cast<uint8_t>(vec4.w * 255.0F)), b(static_cast<uint8_t>(vec4.z * 255.0F)), g(static_cast<uint8_t>(vec4.y * 255.0F)),
        r(static_cast<uint8_t>(vec4.x * 255.0F)) {}

  constexpr explicit PackedColor(const Vec3 &vec3, uint8_t a_val = MaxValue)
      : a(a_val), b(static_cast<uint8_t>(vec3.z * 255.0F)), g(static_cast<uint8_t>(vec3.y * 255.0F)), r(static_cast<uint8_t>(vec3.x * 255.0F)) {}

  constexpr explicit PackedColor(uint32_t rgba) { *this = std::bit_cast<PackedColor>(rgba); }

  [[nodiscard]] auto ToVec4() const -> Vec4 {
    return Vec4{
        static_cast<float>(r) / 255.0F,
        static_cast<float>(g) / 255.0F,
        static_cast<float>(b) / 255.0F,
        static_cast<float>(a) / 255.0F,
    };
  }

  [[nodiscard]] auto ToVec3() const -> Vec3 {
    return Vec3{
        static_cast<float>(r) / 255.0F,
        static_cast<float>(g) / 255.0F,
        static_cast<float>(b) / 255.0F,
    };
  }

  [[nodiscard]] auto ToUint() const -> uint32_t { return std::bit_cast<uint32_t>(*this); }

  [[nodiscard]] auto ToHexString() const -> std::string {
    char buffer[9];
    std::snprintf(buffer, sizeof(buffer), "%02X%02X%02X%02X", r, g, b, a);
    return std::string(buffer);
  }

  [[nodiscard]] auto ToString() const -> std::string {
    return "PackedColor(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ", " + std::to_string(a) + ")";
  }

  [[nodiscard]] auto Hash() const -> uint64_t { return std::hash<uint32_t>()(ToUint()); }

  // NOLINTEND
};

static_assert(sizeof(PackedColor) == sizeof(uint32_t), "PackedColor struct must be 4 bytes in size");
static_assert(std::is_trivially_copyable_v<PackedColor>, "PackedColor struct must be trivially copyable");

} // namespace Math