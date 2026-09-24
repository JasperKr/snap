SnapEngine.keybindings = {}

---@type snap.Keybinding[]
SnapEngine.keybindings.storage = {}

---@type table<string, snap.Keybinding[]>
SnapEngine.keybindings.map_rising = {}

---@type table<string, snap.Keybinding[]>
SnapEngine.keybindings.map_falling = {}

---@type table<string, snap.Keybinding[]>
SnapEngine.keybindings.map_high = {}

---@type table<string, snap.Keybinding[]>
SnapEngine.keybindings.map_low = {}

---@type table<string, function[]>
SnapEngine.keybindings.actions = {}

SnapEngine.keybindings.keysDown = {}

local keybindings = SnapEngine.keybindings

---@alias Key string
---@alias MouseButton number

---@class snap.Keybinding
---@field action string
---@field rising (Key|MouseButton)[]|Key|MouseButton|nil # AKA: On pressed (rising edge)
---@field falling (Key|MouseButton)[]|Key|MouseButton|nil # AKA: On released (falling edge)
---@field high (Key|MouseButton)[]|Key|MouseButton|nil # AKA: While pressed
---@field low (Key|MouseButton)[]|Key|MouseButton|nil # AKA: While released

--- Registers a key binding
--- @param binding snap.Keybinding
function SnapEngine.keybindings.addBinding(binding)
  table.insert(keybindings.storage, binding);

  if type(binding.rising) == "table" then
    for _, key in ipairs(binding.rising or {}) do
      keybindings.map_rising[key] = keybindings.map_rising[key] or {}
      table.insert(keybindings.map_rising[key], binding)
    end
  elseif binding.rising then
    keybindings.map_rising[binding.rising] = binding
    table.insert(keybindings.map_rising[binding.rising], binding)
  end

  if type(binding.falling) == "table" then
    for _, key in ipairs(binding.falling or {}) do
      keybindings.map_falling[key] = keybindings.map_falling[key] or {}
      table.insert(keybindings.map_falling[key], binding)
    end
  elseif binding.falling then
    keybindings.map_falling[binding.falling] = binding
    table.insert(keybindings.map_falling[binding.falling], binding)
  end

  if type(binding.high) == "table" then
    for _, key in ipairs(binding.high or {}) do
      keybindings.map_high[key] = keybindings.map_high[key] or {}
      table.insert(keybindings.map_high[key], binding)
    end
  elseif binding.high then
    keybindings.map_high[binding.high] = binding
    table.insert(keybindings.map_high[binding.high], binding)
  end

  if type(binding.low) == "table" then
    for _, key in ipairs(binding.low or {}) do
      keybindings.map_low[key] = keybindings.map_low[key] or {}
      table.insert(keybindings.map_low[key], binding)
    end
  elseif binding.low then
    keybindings.map_low[binding.low] = binding
    table.insert(keybindings.map_low[binding.low], binding)
  end
end

--- Adds a callback to be called for a given string match
---@param action string
---@param callback function
function SnapEngine.keybindings.addAction(action, callback)
  keybindings.actions[action] = keybindings.actions[action] or {}
  table.insert(keybindings.actions[action], callback)
end

function SnapEngine.keybindings.pressed(key)
  keybindings.keysDown[key] = true

  if keybindings.map_rising[key] == nil then return end

  for _, binding in ipairs(keybindings.map_rising[key]) do
    local callbacks = keybindings.actions[binding.action]

    if callbacks == nil then
      goto continue
    end

    for _, callback in ipairs(callbacks) do
      callback()
    end

    ::continue::
  end
end

function SnapEngine.keybindings.released(key)
  keybindings.keysDown[key] = nil

  if keybindings.map_falling[key] == nil then return end

  for _, binding in ipairs(keybindings.map_falling[key]) do
    local callbacks = keybindings.actions[binding.action]

    if callbacks == nil then
      goto continue
    end

    for _, callback in ipairs(callbacks) do
      callback()
    end

    ::continue::
  end
end

function SnapEngine.keybindings.runCallbacks()
  for index, bindings in pairs(SnapEngine.keybindings.map_low) do
    if keybindings.keysDown[index] then
      goto continue
    end

    for _, binding in ipairs(bindings) do
      for _, callback in pairs(SnapEngine.keybindings.actions[binding.action]) do
        callback();
      end
    end

    ::continue::
  end

  for index, bindings in pairs(SnapEngine.keybindings.map_high) do
    if not keybindings.keysDown[index] then
      goto continue
    end

    for _, binding in ipairs(bindings) do
      for _, callback in pairs(SnapEngine.keybindings.actions[binding.action]) do
        callback();
      end
    end

    ::continue::
  end
end
