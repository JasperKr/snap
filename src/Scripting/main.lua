local i = 0
snap.threaderror = print
print("Starting main thread")
require("init")

local commandsChannel = snap.thread.newChannel()
local startThreadChannel = snap.thread.newChannel()
local events = snap.thread.newChannel()
local scene = snap.scene.newScene("Main")

snap.graphics.acquireGraphics("load")
snap.renderer.initialize()

local thread = snap.thread.newThread("src/Scripting/thread.lua", "Render thread 1")

local qx, qy, qz, qw = snap.math.eulerToQuaternion(-math.pi / 1.5, 0.3, 0);
scene:newDirectionalLight("Test directional light", qx, qy, qz, qw, 1, 1, 1, 5)

print("Starting render thread")
thread:start(commandsChannel, startThreadChannel, scene, events)

local buffer = require("string.buffer");

function snap.any(...)
  events:push(buffer.encode({ ... }))
end

function snap.update(dt)
  i = i + 1

  scene:update(dt)
end

function snap.mousemoved(x, y, dx, dy)
  snap.gui.mouseMoved(x, y)
end

function snap.mousepressed(x, y, button, istouch, presses)
  snap.gui.mousePressed(x, y, button)
end

function snap.mousereleased(x, y, button, istouch, presses)
  snap.gui.mouseReleased(x, y, button)
end

function snap.keypressed(key, scancode, isrepeat)
  -- ctrl + alt + c = capture
  if (snap.keyboard.isDown("lctrl") and snap.keyboard.isDown("lalt") and key == "c") then
    snap.filesystem.write("capture", "")
    print(snap.filesystem.getSaveDirectory() .. "/capture created")
  end

  snap.gui.keyPressed(key)
end

function snap.keyreleased(key, scancode)
  snap.gui.keyReleased(key)
end

function snap.textinput(text)
  snap.gui.textInput(text)
end

function snap.wheelmoved(x, y)
  snap.gui.mouseWheelMoved(x, y)
end

function snap.quit()
  print("main thread received quit signal")

  startThreadChannel:push(false)
  startThreadChannel:push(false)

  thread:wait()
  snap.gui.shutdown()

  print("Stopping main thread.")
  return 1
end

snap.keyboard.setEnableTextInput(true)

local firstFrame = true
local commandBuffers = {}

function snap.draw()
  startThreadChannel:push(true)

  table.clear(commandBuffers)

  if firstFrame then
    table.insert(commandBuffers, snap.graphics.submitGraphics())
    firstFrame = false
  end

  local gotBuffer = false

  while not gotBuffer do
    if thread:getStatus() ~= "running" then
      print(thread:getError())
      snap.event.quit()
      return
    end
    local buffer = commandsChannel:demand(50)
    while buffer do
      table.insert(commandBuffers, buffer)
      buffer = commandsChannel:pop()
      gotBuffer = true
    end
  end

  return commandBuffers
end
