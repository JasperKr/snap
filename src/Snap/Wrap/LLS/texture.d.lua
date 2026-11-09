---@meta Texture

error("Do not require this file")

---@alias snap.Filter
---| "nearest" # Nearest-neighbor filtering
---| "linear" # Linear filtering

---@alias snap.TextureFormat
---| "rgba8" Default format
---| "rgba16"
---| "rgba16f" Medium-precision floating point
---| "rgba32f" High-precision floating point
---| "rgba8ui"
---| "rgba16ui"
---| "rgba32ui"
---| "rgba8si"
---| "rgba16si"
---| "rgba32si"
---| "depth16" Low precision depth
---| "depth24" Medium precision depth
---| "depth32" High precision depth
---| "depth24stencil8"
---| "depth32stencil8"
---| "rg11b10f" HDR rgb-only
---| "rgb9e5"
---| "rgb10a2" High precision rgb, low precision alpha
---| "rgb10a2ui"
---| "bgr5a1"
---| "bgr565"
---| "rgba4"
---| "bc1"
---| "bc3"
---| "bc4"
---| "bc5" Useful for Normal maps
---| "bc6h" Useful for compressed HDR textures
---| "bc6hs"
---| "bc7" Useful for High quality compressed textures

---@alias snap.TextureMipmapOptions
---| "none"   # Do not create mipmaps.
---| "init"   # Calculate a mip chain.
---| "manual" # Allocate a mip chain.

---@class snap.Texture : snap.Data
local Texture = {}

--- Sets the filter mode for the texture.
---@param filtermin snap.Filter The minification filter.
---@param filtermag snap.Filter The magnification filter.
function Texture:setFilter(filtermin, filtermag) end

--- Gets the filter mode for the texture.
---@return snap.Filter filtermin The minification filter.
---@return snap.Filter filtermag The magnification filter.
function Texture:getFilter() end

--- Sets the anisotropic level for the texture.
---@param level number The anisotropic level.
function Texture:setAnisotropy(level) end

--- Gets the anisotropic level for the texture.
---@return number level The anisotropic level.
function Texture:getAnisotropy() end

--- Sets the wrap mode for the texture.
---@param wraps snap.WrapMode The wrap mode for the S (U) coordinate.
---@param wrapt snap.WrapMode The wrap mode for the T (V) coordinate.
function Texture:setWrap(wraps, wrapt) end

--- Gets the wrap mode for the texture.
---@return snap.WrapMode wraps The wrap mode for the S (U) coordinate.
---@return snap.WrapMode wrapt The wrap mode for the T (V) coordinate.
function Texture:getWrap() end

--- Sets the LOD bias for the texture.
---@param bias number The LOD bias.
function Texture:setLODBias(bias) end

--- Gets the LOD bias for the texture.
---@return number bias The LOD bias.
function Texture:getLODBias() end

--- Sets the LOD range for the texture.
---@param min number The minimum LOD.
---@param max number The maximum LOD.
function Texture:setLODRange(min, max) end

--- Gets the LOD range for the texture.
---@return number min The minimum LOD.
---@return number max The maximum LOD.
function Texture:getLODRange() end

---@alias snap.TextureDepthCompareOp
---| "never" # Never pass
---| "less" # Pass if the incoming depth value is less than the stored depth value
---| "equal" # Pass if the incoming depth value is equal to the stored depth value
---| "lequal" # Pass if the incoming depth value is less than or equal to
---| "greater" # Pass if the incoming depth value is greater than the stored depth value
---| "notequal" # Pass if the incoming depth value is not equal total
---| "gequal" # Pass if the incoming depth value is greater than or equal to
---| "always" # Always pass
--- the stored depth value

--- Sets the depth compare mode for the texture.
---@param compareOp snap.TextureDepthCompareOp The depth compare operation.
function Texture:setDepthCompare(compareOp) end

--- Gets the depth compare mode for the texture.
---@return snap.TextureDepthCompareOp compareOp The depth compare operation.
function Texture:getDepthCompare() end

--- Gets the width of the texture.
---@return number width The width of the texture.
function Texture:getWidth() end

--- Gets the height of the texture.
---@return number height The height of the texture.
function Texture:getHeight() end

--- Gets the dimensions of the texture.
---@return number width The width of the texture.
---@return number height The height of the texture.
function Texture:getDimensions() end

--- Gets the depth of the texture.
---@return number depth The depth of the texture.
function Texture:getDepth() end

--- Gets the mipmap count of the texture.
---@return number mipmapCount The mipmap count of the texture.
function Texture:getMipmapCount() end

--- Gets the format of the texture.
---@return snap.TextureFormat format The format of the texture.
function Texture:getFormat() end
