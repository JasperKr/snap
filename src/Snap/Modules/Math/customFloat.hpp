#include <bit>
#include <cstdint>

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

template <typename Storage, int EXP_BITS, int MANT_BITS, bool SIGNED> struct CustomFloat {
  static constexpr uint32_t exp_bits = EXP_BITS;
  static constexpr uint32_t mant_bits = MANT_BITS;
  static constexpr bool has_sign = SIGNED;

  static constexpr int sign_bits = SIGNED ? 1 : 0;
  static constexpr int total_bits = sign_bits + exp_bits + mant_bits;

  static constexpr int exp_bias = (1U << (exp_bits - 1U)) - 1U;

  using storage_t = Storage;

  struct value {
    storage_t bits;
  };

  // ---- encode from float32 ----
  static auto fromFloat(float float_value) -> value {
    auto ufloat = std::bit_cast<uint32_t>(float_value);

    // uint32_t sign = SIGNED ? (ufloat >> 31U) & 1U : 0U;
    uint32_t sign{};
    if constexpr (SIGNED) {
      sign = (ufloat >> 31U) & 1U;
    }

    int32_t exp = static_cast<int32_t>((ufloat >> 23U) & 0xFFU) - 127;
    uint32_t mant = ufloat & 0x7FFFFFU;

    storage_t out = 0;

    // normalize exponent into new range
    int32_t newExp = exp + exp_bias;

    if (newExp > 0) {
      if (newExp < (1U << exp_bits) - 1U) {
        // shrink mantissa
        mant >>= (23U - mant_bits);
      } else {
        // overflow -> max (inf)
        newExp = (1U << exp_bits) - 1U;
        mant = 0;
      }
    } else {
      // underflow -> zero
      newExp = 0;
      mant = 0;
    }

    out |= mant;
    out |= (storage_t)newExp << mant_bits;
    if constexpr (SIGNED) {
      out |= (storage_t)sign << (mant_bits + exp_bits);
    }

    return {out};
  }

  // ---- decode to float32 ----
  static auto toFloat(value float_value) -> float {
    storage_t bits = float_value.bits;

    uint32_t mant = bits & ((1U << mant_bits) - 1U);
    uint32_t exp = (bits >> mant_bits) & ((1U << exp_bits) - 1U);
    uint32_t sign = SIGNED ? (bits >> (mant_bits + exp_bits)) & 1U : 0U;

    uint32_t out = 0U;

    if (exp == 0U) {
      // zero / subnormal -> zero
      out = 0U;
    } else {
      if (exp == (1U << exp_bits) - 1) {
        // inf
        out = (sign << 31U) | (0xFFU << 23U);
      } else {
        int32_t exponent = (int)exp - exp_bias + 127;
        uint32_t mantissa = mant << (23U - mant_bits);

        out = (sign << 31U) | ((uint32_t)exponent << 23U) | mantissa;
      }
    }

    return std::bit_cast<float>(out);
  }
};

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)