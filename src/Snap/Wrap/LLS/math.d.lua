---@meta Math

error("Do not require this file")

snap.math = {}

--- Converts an euler angle to a quaternion
--- @param pitch number
--- @param yaw number
--- @param roll number
--- @return number x
--- @return number y
--- @return number z
--- @return number w
function snap.math.eulerToQuaternion(pitch, yaw, roll) end

--- Converts an euler angle to a matrix4x4
--- @param pitch number
--- @param yaw number
--- @param roll number
--- @return ... matrix
function snap.math.eulerToMatrix(pitch, yaw, roll) end

--- Converts a quaternion to an euler angle
--- @param x number
--- @param y number
--- @param z number
--- @param w number
--- @return number yaw
--- @return number pitch
--- @return number roll
function snap.math.quaternionToEuler(x, y, z, w) end

--- Converts a quaternion to a matrix4x4
--- @param x number
--- @param y number
--- @param z number
--- @param w number
--- @return number[16] matrix
function snap.math.quaternionToMatrix(x, y, z, w) end

--- Converts a matrix4x4 to an euler angle
--- @param matrix number[16]
--- @return number yaw
--- @return number pitch
--- @return number roll
function snap.math.matrixToEuler(matrix) end

--- Converts a matrix4x4 to a quaternion
--- @param matrix number[16]
--- @return number x
--- @return number y
--- @return number z
--- @return number w
function snap.math.matrixToQuaternion(matrix) end

--[[
auto wrap_TranslationMatrix(lua_State *state) -> int;
auto wrap_ScaleMatrix(lua_State *state) -> int;
auto wrap_TransformMatrix(lua_State *state) -> int;
]]

--- Creates a translation matrix
--- @param x number
--- @param y number
--- @param z number
--- @return number[16] matrix
function snap.math.translationMatrix(x, y, z) end

--- Creates a scale matrix
--- @param x number
--- @param y number
--- @param z number
--- @return number[16] matrix
function snap.math.scaleMatrix(x, y, z) end

--- Creates a transform matrix from a translation, rotation and scale
--- @param translationX number
--- @param translationY number
--- @param translationZ number
--- @param scaleX number
--- @param scaleY number
--- @param scaleZ number
--- @param rotationX number
--- @param rotationY number
--- @param rotationZ number
--- @return number[16] matrix
function snap.math.transformMatrix(translationX, translationY, translationZ, scaleX, scaleY, scaleZ, rotationX, rotationY,
                                   rotationZ)
end

--- Returns a random integer between min and max
--- @overload fun(max: integer): integer Returns a random integer between 0 and max inclusive
--- @overload fun(): number Returns a random float between 0 and 1
--- @param min integer
--- @param max integer
--- @return integer random
function snap.math.random(min, max) end

--- Returns a noise value between -1 and 1 for the given channels
--- @overload fun(x: number, y: number): number Returns a noise value for 2D coordinates
--- @overload fun(x: number): number Returns a noise value for 1D coordinates
--- @param x number
--- @param y number
--- @param z number
--- @return number noise
function snap.math.noise(x, y, z) end

--- Returns a noise value between -1 and 1 for the given channels, wrapping around at the given period
--- @overload fun(x: number, y: number, xWrapping: number, yWrapping: number): number Returns a noise value for 2D coordinates
--- @overload fun(x: number, xWrapping: number): number Returns a noise value for 1D coordinates
--- @param x number
--- @param y number
--- @param z number
--- @param xWrapping number
--- @param yWrapping number
--- @param zWrapping number
--- @return number noise
function snap.math.noiseWrapped(x, y, z, xWrapping, yWrapping, zWrapping) end
