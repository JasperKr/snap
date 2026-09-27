SnapEngine.keybindings = {}

---@type snap.Keybinding[]
SnapEngine.keybindings.bindings = {}

---@type table<string, {[1]:function, [2]:any}[]>
SnapEngine.keybindings.actions = {}

SnapEngine.keybindings.keysDown = {}
SnapEngine.keybindings.keysRising = {}
SnapEngine.keybindings.keysFalling = {}

local keybindings = SnapEngine.keybindings

---@alias Key string
---@alias MouseButton number

--- Default state is any
---@alias snap.KeyModState "pressed"|"released"|"any"?

---@class snap.Keybinding
---@field action string
---@field rising (Key|MouseButton)[]|Key|MouseButton|nil # AKA: On pressed (rising edge)
---@field falling (Key|MouseButton)[]|Key|MouseButton|nil # AKA: On released (falling edge)
---@field high (Key|MouseButton)[]|Key|MouseButton|nil # AKA: While pressed
---@field low (Key|MouseButton)[]|Key|MouseButton|nil # AKA: While released

--- Check if a binding should be ran
---@param binding snap.Keybinding
---@return boolean
local function bindingActive(binding)
  for i, key in ipairs(binding.high) do if not keybindings.keysDown[key] then return false end end
  for i, key in ipairs(binding.low) do if keybindings.keysDown[key] then return false end end
  for i, key in ipairs(binding.rising) do if not keybindings.keysRising[key] then return false end end
  for i, key in ipairs(binding.falling) do if not keybindings.keysFalling[key] then return false end end

  return true
end

--- Registers a key binding
--- @param binding snap.Keybinding
function SnapEngine.keybindings.addBinding(binding)
  if binding.falling and type(binding.falling) ~= "table" then
    binding.falling = { binding.falling }
  else
    binding.falling = {}
  end

  if binding.rising and type(binding.rising) ~= "table" then
    binding.rising = { binding.rising }
  else
    binding.rising = {}
  end

  if binding.high and type(binding.high) ~= "table" then
    binding.high = { binding.high }
  else
    binding.high = {}
  end

  if binding.low and type(binding.low) ~= "table" then
    binding.low = { binding.low }
  else
    binding.low = {}
  end

  table.insert(keybindings.bindings, binding);
end

--- Adds a callback to be called for a given string match
---@param action string
---@param callback function
---@param ... any
function SnapEngine.keybindings.addAction(action, callback, ...)
  assert(callback, action)
  keybindings.actions[action] = keybindings.actions[action] or {}
  table.insert(keybindings.actions[action], { callback, ... })
end

function SnapEngine.keybindings.pressed(key)
  keybindings.keysDown[key] = true
  keybindings.keysRising[key] = true
end

function SnapEngine.keybindings.released(key)
  keybindings.keysDown[key] = nil
  keybindings.keysFalling[key] = true
end

function SnapEngine.keybindings.runCallbacks()
  for index, binding in pairs(SnapEngine.keybindings.bindings) do
    if bindingActive(binding) then
      for _, action in ipairs(SnapEngine.keybindings.actions[binding.action] or {}) do
        action[1](unpack(action, 2))
      end
    end
  end

  table.clear(keybindings.keysFalling)
  table.clear(keybindings.keysRising)
end
