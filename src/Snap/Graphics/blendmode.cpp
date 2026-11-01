#include "blendmode.hpp"

#include <format>
#include <string>
#include <vulkan/vulkan_core.h>

namespace Graphics::BlendMode {

auto ToString(VkBlendFactor blendFactor) -> std::string_view {
  switch (blendFactor) {
  case VK_BLEND_FACTOR_ZERO:
    return "zero";
  case VK_BLEND_FACTOR_ONE:
    return "one";
  case VK_BLEND_FACTOR_SRC_COLOR:
    return "src_color";
  case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
    return "one_minus_src_color";
  case VK_BLEND_FACTOR_DST_COLOR:
    return "dst_color";
  case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
    return "one_minus_dst_color";
  case VK_BLEND_FACTOR_SRC_ALPHA:
    return "src_alpha";
  case VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
    return "one_minus_src_alpha";
  case VK_BLEND_FACTOR_DST_ALPHA:
    return "dst_alpha";
  case VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
    return "one_minus_dst_alpha";
  case VK_BLEND_FACTOR_SRC_ALPHA_SATURATE:
    return "src_alpha_saturate";
  case VK_BLEND_FACTOR_SRC1_COLOR:
    return "src1_color";
  case VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
    return "one_minus_src1_color";
  case VK_BLEND_FACTOR_SRC1_ALPHA:
    return "src1_alpha";
  case VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
    return "one_minus_src1_alpha";
  case VK_BLEND_FACTOR_CONSTANT_COLOR:
    return "constant_color";
  case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
    return "one_minus_constant_color";
  case VK_BLEND_FACTOR_CONSTANT_ALPHA:
    return "constant_alpha";
  case VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
    return "one_minus_constant_alpha";
  case VK_BLEND_FACTOR_MAX_ENUM:
    break;
  }

  return "unknown blend factor";
}
auto ToString(VkBlendOp blendOp) -> std::string_view {
  switch (blendOp) {
  case VK_BLEND_OP_ADD:
    return "add";
  case VK_BLEND_OP_SUBTRACT:
    return "subtract";
  case VK_BLEND_OP_REVERSE_SUBTRACT:
    return "reverse_subtract";
  case VK_BLEND_OP_MIN:
    return "min";
  case VK_BLEND_OP_MAX:
    return "max";
  default:
    break;
  }

  return "unknown blend op";
}

/*
if (strcmp(modeStr, "none") == 0) {
  blendMode.blendEnable = VK_FALSE;
} else if (strcmp(modeStr, "alpha") == 0) {
  blendMode.blendEnable = VK_TRUE;
  blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blendMode.colorBlendOp = VK_BLEND_OP_ADD;
  blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
} else if (strcmp(modeStr, "add") == 0) {
  blendMode.blendEnable = VK_TRUE;
  blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.colorBlendOp = VK_BLEND_OP_ADD;
  blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
} else if (strcmp(modeStr, "sub") == 0) {
  blendMode.blendEnable = VK_TRUE;
  blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.colorBlendOp = VK_BLEND_OP_SUBTRACT;
  blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  blendMode.alphaBlendOp = VK_BLEND_OP_SUBTRACT;
} else if (strcmp(modeStr, "mul") == 0) {
  blendMode.blendEnable = VK_TRUE;
  blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
  blendMode.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendMode.colorBlendOp = VK_BLEND_OP_ADD;
  blendMode.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
  blendMode.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendMode.alphaBlendOp = VK_BLEND_OP_ADD;
} else {
  return Error::Unexpected("Invalid blend mode: " + std::string(modeStr));
}

const char *alphaModeStr = luaL_checkstring(state, -1);
if (strcmp(alphaModeStr, "alphamultiply") == 0) {
} else if (strcmp(alphaModeStr, "premultiplied") == 0) {
  blendMode.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
} else {
  return Error::Unexpected("Invalid alpha blend mode: " +
                            std::string(alphaModeStr));
}
*/

/*
alpha,
add,
subtract,
multiply,
replace
*/

auto ToString(VkBlendFactor srcColorBlendFactor,
              VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp,
              VkBlendFactor srcAlphaBlendFactor,
              VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp)
    -> std::tuple<bool, std::string, std::string> {

  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "alpha", "premultiplied"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "add", "premultiplied"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      colorBlendOp == VK_BLEND_OP_SUBTRACT &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      alphaBlendOp == VK_BLEND_OP_SUBTRACT) {
    return {true, "subtract", "premultiplied"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_DST_COLOR &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ZERO &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_DST_ALPHA &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "multiply", "premultiplied"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ZERO &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "replace", "premultiplied"};
  }

  if (srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "alpha", "alphamultiply"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "add", "alphamultiply"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      colorBlendOp == VK_BLEND_OP_SUBTRACT &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      alphaBlendOp == VK_BLEND_OP_SUBTRACT) {
    return {true, "subtract", "alphamultiply"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_DST_COLOR &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ZERO &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_DST_ALPHA &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "multiply", "alphamultiply"};
  }
  if (srcColorBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstColorBlendFactor == VK_BLEND_FACTOR_ZERO &&
      colorBlendOp == VK_BLEND_OP_ADD &&
      srcAlphaBlendFactor == VK_BLEND_FACTOR_ONE &&
      dstAlphaBlendFactor == VK_BLEND_FACTOR_ZERO &&
      alphaBlendOp == VK_BLEND_OP_ADD) {
    return {true, "replace", "alphamultiply"};
  }

  return {false, "", ""};
}

auto ToStringProperties(VkPipelineColorBlendAttachmentState state)
    -> std::tuple<bool, std::string, std::string> {
  if (state.blendEnable == VK_FALSE) {
    return {true, "none", ""};
  }

  return ToString(state.srcColorBlendFactor, state.dstColorBlendFactor,
                  state.colorBlendOp, state.srcAlphaBlendFactor,
                  state.dstAlphaBlendFactor, state.alphaBlendOp);
}

auto ToString(VkPipelineColorBlendAttachmentState state) -> std::string {
  if (state.blendEnable == VK_FALSE) {
    return "Blend mode: none";
  }

  const auto &[simple, blendmode, alphamode] = ToString(
      state.srcColorBlendFactor, state.dstColorBlendFactor, state.colorBlendOp,
      state.srcAlphaBlendFactor, state.dstAlphaBlendFactor, state.alphaBlendOp);

  if (simple) {
    return std::format("Blend Mode: {}, Alpha Mode: {}", blendmode, alphamode);
  }

  return std::format("Blend State:\nSrc-Color={}\nDst-Color={}\nColor Op={}"
                     "\nSrc Alpha={}\nDst Alpha={}\nAlpha Op={}",
                     BlendMode::ToString(state.srcColorBlendFactor),
                     BlendMode::ToString(state.dstColorBlendFactor),
                     BlendMode::ToString(state.colorBlendOp),
                     BlendMode::ToString(state.srcAlphaBlendFactor),
                     BlendMode::ToString(state.dstAlphaBlendFactor),
                     BlendMode::ToString(state.alphaBlendOp));
}

} // namespace Graphics::BlendMode