---@meta Timer

error("Do not require this file")

-- Timer module for snap

snap.timer = {}

--- Gets the current time in seconds.
--- @return number Current time in seconds.
function snap.timer.getTime() end

--- Gets the time elapsed since the last frame in seconds.
--- @return number Delta time in seconds.
function snap.timer.getDelta() end

--- Gets the current frames per second (FPS).
--- @return number Current FPS.
function snap.timer.getFPS() end

--- Sleeps the current thread for the specified duration in seconds.
--- @param seconds number Duration to sleep in seconds.
function snap.timer.sleep(seconds) end

--- Gets the average delta time.
--- @return number Average delta time in seconds.
function snap.timer.getAverageDelta() end

--- Steps the timer one frame. Not thread-local.
function snap.timer.step() end
