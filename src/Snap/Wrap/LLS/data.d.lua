---@meta Data

error("Do not require this file")

--- Global Data class
---@class snap.Data
Data = {}

snap.data = {}

--- Releases the data from memory.
function Data:release() end

---@return string
function Data:getType() end

--- ByteData class
---@class snap.Bytedata : snap.Data
local Bytedata = {}

--- Gets the size of the Bytedata in bytes.
---@return number size The size in bytes.
function Bytedata:getSize() end

--- Gets a pointer to the Bytedata.
---@return integer pointer The pointer to the Bytedata.
function Bytedata:getPointer() end

--- Sets a half-precision float at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The half-precision float value.
function Bytedata:setHalf(offset, value) end

--- Getst half-precision floats at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of half-precision floats to get.
---@return number ... The half-precision float values.
function Bytedata:getHalf(offset, count) end

--- Sets a float at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The float value.
function Bytedata:setFloat(offset, value) end

--- Gets floats at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of floats to get.
---@return number ... The float values.
function Bytedata:getFloat(offset, count) end

--- Sets an unsigned 32-bit integer at the specified offset.
-- ---@overload fun(offset: number, values: number[]): nil
-- ---@param offset number The offset in bytes.
-- ---@param value number The unsigned 32-bit integer value.
-- function Bytedata:setUInt32(offset, value) end

--- Gets unsigned 32-bit integers at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of unsigned 32-bit integers to get.
---@return number ... The unsigned 32-bit integer values.
function Bytedata:getUInt32(offset, count) end

--- Sets a signed 32-bit integer at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The signed 32-bit integer value.
function Bytedata:setInt32(offset, value) end

--- Gets signed 32-bit integers at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of signed 32-bit integers to get.
---@return number ... The signed 32-bit integer values.
function Bytedata:getInt32(offset, count) end

--- Sets an unsigned 16-bit integer at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The unsigned 16-bit integer value.
function Bytedata:setUInt16(offset, value) end

--- Gets unsigned 16-bit integers at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of unsigned 16-bit integers to get.
---@return number ... The unsigned 16-bit integer values.
function Bytedata:getUInt16(offset, count) end

--- Sets a signed 16-bit integer at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The signed 16-bit integer value.
function Bytedata:setInt16(offset, value) end

--- Gets signed 16-bit integers at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of signed 16-bit integers to get.
---@return number ... The signed 16-bit integer values.
function Bytedata:getInt16(offset, count) end

--- Sets an unsigned 8-bit integer at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The unsigned 8-bit integer value.
function Bytedata:setUInt8(offset, value) end

--- Gets unsigned 8-bit integers at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of unsigned 8-bit integers to get.
---@return number ... The unsigned 8-bit integer values.
function Bytedata:getUInt8(offset, count) end

--- Sets a signed 8-bit integer at the specified offset.
---@overload fun(offset: number, values: number[]): nil
---@param offset number The offset in bytes.
---@param value number The signed 8-bit integer value.
function Bytedata:setInt8(offset, value) end

--- Gets signed 8-bit integers at the specified offset.
---@param offset number The offset in bytes.
---@param count number The number of signed 8-bit integers to get.
---@return number ... The signed 8-bit integer values.
function Bytedata:getInt8(offset, count) end

--- Creates a new Bytedata object.
---@param size number The size in bytes.
---@return snap.Bytedata bytedata The new Bytedata object.
function snap.data.newBytedata(size) end
