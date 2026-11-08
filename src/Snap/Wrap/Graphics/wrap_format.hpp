#pragma once

#include "Graphics/bufferformat.hpp"

#include "lua.hpp"

namespace Wrap::Graphics {

auto FormatFromLua(lua_State *state, int index, ::Graphics::Standard standard = ::Graphics::Standard::Std430) -> Result<::Graphics::BufferFormat>;
auto SimpleFormatFromLua(lua_State *state, int index, ::Graphics::Standard standard = ::Graphics::Standard::Std430) -> Result<::Graphics::BufferFormat>;

} // namespace Wrap::Graphics
