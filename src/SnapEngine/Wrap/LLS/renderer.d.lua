---@meta Renderer

error("Do not require this file")

snap.renderer = {}

function snap.renderer.initialize() end

function snap.renderer.reloadShaders() end

---@class snap.Camera
local Camera = {}

function Camera:getAspectRatio() end

function Camera:setAspectRatio(aspectRatio) end

function Camera:getVerticalFOV() end

function Camera:setVerticalFOV(fov) end

function Camera:getNearPlane() end

function Camera:setNearPlane(near) end

function Camera:getFarPlane() end

function Camera:setFarPlane(far) end

function Camera:getBuffer() end

function Camera:render() end

function Camera:getRendertarget(name) end

function Camera:getDimensions() end

function Camera:setDimensions(width, height) end

function Camera:getWidth() end

function Camera:setWidth(width) end

function Camera:getHeight() end

function Camera:setHeight(height) end
