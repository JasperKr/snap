---@meta Gui

error("Do not require this file")

snap.gui = {}

--- Creates a new frame
--- @param dt number Delta time
function snap.gui.newFrame(dt) end

--- Ends the current frame
function snap.gui.endFrame() end

--- Renders the current frame
function snap.gui.draw() end

--[[
    {"mousePressed", MousePressed}, -- x, y, button
    {"mouseReleased", MouseReleased}, -- x, y, button
    {"mouseMoved", MouseMoved}, -- x, y
    {"mouseWheelMoved", MouseWheelMoved}, -- x, y
    {"keyPressed", KeyPressed}, -- key
    {"keyReleased", KeyReleased}, -- key
    {"textInput", TextInput}, -- text
]]

--- Handles a mouse pressed event
--- @param x number X position
--- @param y number Y position
--- @param button number Button index
function snap.gui.mousePressed(x, y, button) end

--- Handles a mouse released event
--- @param x number X position
--- @param y number Y position
--- @param button number Button index
function snap.gui.mouseReleased(x, y, button) end

--- Handles a mouse moved event
--- @param x number X position
--- @param y number Y position
function snap.gui.mouseMoved(x, y) end

--- Handles a mouse wheel moved event
--- @param x number X position
--- @param y number Y position
function snap.gui.mouseWheelMoved(x, y) end

--- Handles a key pressed event
--- @param key number Key code
function snap.gui.keyPressed(key) end

--- Handles a key released event
--- @param key number Key code
function snap.gui.keyReleased(key) end

--- Handles a text input event
--- @param text string Input text
function snap.gui.textInput(text) end
