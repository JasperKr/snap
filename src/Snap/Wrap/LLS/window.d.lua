---@meta Timer

error("Do not require this file")

snap.window = {}

--- Hides the window.
function snap.window.hide() end

--- Returns the number of available displays.
---@return integer
function snap.window.getDisplayCount() end

--- Returns the name of the specified display.
---@param displayIndex integer
---@return string
function snap.window.getDisplayName(displayIndex) end

--- Returns whether the window is in fullscreen mode.
---@return boolean
function snap.window.isFullscreen() end

--- Returns the dimensions of the specified fullscreen display.
---@param displayIndex integer
---@return integer, integer
function snap.window.getFullscreenDimensions(displayIndex) end

--- Sets whether the window is in fullscreen mode.
---@param fullscreen boolean
function snap.window.setFullscreen(fullscreen) end

--- Returns the width of the window.
---@return integer
function snap.window.getWidth() end

--- Returns the height of the window.
---@return integer
function snap.window.getHeight() end

--- Returns the dimensions of the window.
---@return integer, integer
function snap.window.getDimensions() end

--- Sets the dimensions of the window.
---@param width integer
---@param height integer
function snap.window.setDimensions(width, height) end

--- Sets the icon of the window.
---@param imageData string
function snap.window.setIcon(imageData) end

---@alias snap.VsyncMode
---| "enabled" # Standard VSync enabled.
---| "immediate" # VSync disabled. Tearing may occur.
---| "adaptive" # Adaptive VSync. Disable Vsync when framerate is lower than the monitor refreshrate.
---| "replace" # Vsync enabled, no framerate limit.

---@class snap.WindowSettings
---@field vsync snap.VsyncMode? VSync mode.
---@field resizable boolean? Whether the window is resizable.
---@field borderless boolean? Whether the window is borderless.
---@field fullscreen boolean? Whether the window is fullscreen.
---@field displayIndex integer? The display index to use.
---@field width integer? The window width.
---@field height integer? The window height.
---@field minimumWidth integer? The minimum window width.
---@field minimumHeight integer? The minimum window height.
---@field xPosition integer? The X position of the window.
---@field yPosition integer? The Y position of the window.

--- Returns the current window settings.
---@return snap.WindowSettings
function snap.window.getSettings() end

--- Sets the window settings.
---@param settings snap.WindowSettings
function snap.window.setSettings(settings) end

--- Updates specific window settings.
---@param settings snap.WindowSettings
function snap.window.updateSettings(settings) end

--- Returns the window title.
---@return string
function snap.window.getTitle() end

--- Sets the window title.
---@param title string
function snap.window.setTitle(title) end

--- Returns the window position.
---@return integer, integer
function snap.window.getPosition() end

--- Sets the window position.
---@param x integer
---@param y integer
function snap.window.setPosition(x, y) end

--- Sets the VSync mode.
---@param mode snap.VsyncMode
function snap.window.setVSync(mode) end

--- Returns whether the window has mouse focus.
---@return boolean
function snap.window.hasMouseFocus() end

--- Returns whether the window has keyboard focus.
---@return boolean
function snap.window.hasKeyboardFocus() end

--- Returns whether the window is visible.
---@return boolean
function snap.window.isVisible() end

--- Returns whether the window is open.
---@return boolean
function snap.window.isOpen() end

--- Returns whether the window is minimized.
---@return boolean
function snap.window.isMinimized() end

--- Returns whether the window is maximized.
---@return boolean
function snap.window.isMaximized() end

--- Minimizes the window.
function snap.window.minimize() end

--- Maximizes the window.
function snap.window.maximize() end

--- Restores the window from minimized or maximized state.
function snap.window.restore() end

--- Enables or disables display sleep.
---@param enable boolean
function snap.window.enableDisplaySleep(enable) end

--- Returns whether display sleep is enabled.
---@return boolean
function snap.window.isDisplaySleepEnabled() end

--- Requests user attention to the window.
---@param continuous boolean Whether to continuously request attention until the window gains focus.
function snap.window.requestAttention(continuous) end
