---@meta Keyboard

error("Do not require this file")

snap.keyboard = {}

--- Checks if a key is currently being pressed
---@param ... string
---@return boolean anyDown
---@return boolean ... vararg returns the state of each key
function snap.keyboard.isDown(...) end

--- Checks if a scancode is currently being pressed
---@param ... string
---@return boolean anyDown
---@return boolean ... vararg returns the state of each scancode
function snap.keyboard.isScancodeDown(...) end

--- Enables or disables text input events
---@param enable boolean
function snap.keyboard.setEnableTextInput(enable) end

--- Checks if text input events are enabled
---@return boolean enabled
function snap.keyboard.isTextInputEnabled() end
