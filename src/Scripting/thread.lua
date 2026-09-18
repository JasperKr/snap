require("init")
local ffi = require("ffi")
local buffer = require("string.buffer")

local lastDrawTime = 0
local lastImDrawTime = 0
local lastShownTime = 0
local lastShownImDrawTime = 0
local count = 0
local viewport = { offset = vec2(), size = vec2() }

local commandBufferChannel, canStartChannel, scene, events = ...

-- scene:newCamera(name, verticalFOV, width, height, near, far)
local camera = scene:newCamera("main camera", 90, 1, 1, 0.01, 1000)
camera:setPersistentTextureSettings({
  PostProcessed = true,
})

snap.renderer.setEditorCamera(camera)

local probe = scene:newLightProbe()

local snapshot

local function draw()
  snap.graphics.setCullMode("none")
  snap.graphics.setDepthMode("greater", true)

  Editor.drawGUI()

  camera:render(scene)
  snap.graphics.setShader()

  if Imgui.Begin("Debug Info") then
    Imgui.Text(string.format("Frame time: %.3f ms", snap.timer.getDelta() * 1000))
    Imgui.Text(string.format("FPS: %.1f", snap.timer.getFPS()))
    Imgui.Text(string.format("ImGui draw time: %.3f ms", lastShownImDrawTime * 1000))

    local stats = snap.graphics.getStats()

    Imgui.Text(string.format("Draw calls: %d", stats.drawcalls))
    Imgui.Text(string.format("Dispatches: %d", stats.dispatches))
    Imgui.Text(string.format("Triangles: %d", stats.triangles))
    Imgui.Text(string.format("Instances: %d", stats.instances))
    Imgui.Text(string.format("Context switches: %d", stats.contextswitches))
    Imgui.Text(string.format("Texture memory: %.2f MiB", stats.texturememory / (1024 * 1024)))
    Imgui.Text(string.format("Buffer memory: %.2f MiB", stats.buffermemory / (1024 * 1024)))
    Imgui.Text(string.format("BLAS memory: %.2f MiB", stats.blasmemory / (1024 * 1024)))
    Imgui.Text(string.format("TLAS memory: %.2f MiB", stats.tlasmemory / (1024 * 1024)))
  end
  Imgui.End()

  Imgui.SetNextWindowDockID(Editor.dockId, Imgui.ImGuiCond_FirstUseEver)

  local flags = bit.bor(Imgui.ImGuiWindowFlags_NoScrollbar, Imgui.ImGuiWindowFlags_NoScrollWithMouse)
  if Imgui.Begin("Viewport", nil, flags) then
    local windowSize = Imgui.GetWindowSize()
    Imgui.SetCursorPos(ffi.new("ImVec2", 0, 0))
    local rt = camera:getRendertarget("PostProcessed")
    Imgui.Image(rt, windowSize)
    rt:release()

    camera:setDimensions(windowSize.x, windowSize.y)

    viewport.offset.x = Imgui.GetWindowPos().x
    viewport.offset.y = Imgui.GetWindowPos().y
    viewport.size.x = windowSize.x
    viewport.size.y = windowSize.y
  end
  Imgui.End()

  scene:drawUIElement();

  local startTime = snap.timer.getTime()

  if snapshot then
    snapshot:draw()
  end

  snap.gui.endFrame()
  local imStartTime = snap.timer.getTime()

  ---@type snap.DetailedBlendMode
  local imguiBlendState = {
    alphaop = "add",
    colorop = "add",
    srcalpha = "srcalpha",
    srccolor = "srcalpha",
    dstalpha = "oneminussrcalpha",
    dstcolor = "oneminussrcalpha",
  }

  snap.graphics.setRenderTarget({ loadas = "clear", blendmode = imguiBlendState })
  snap.gui.draw()
  snap.graphics.setScissor();
  lastImDrawTime = lastImDrawTime + snap.timer.getTime() - imStartTime

  lastDrawTime = lastDrawTime + snap.timer.getTime() - startTime
  count = count + 1
  if (count >= 50) then
    lastShownTime = lastDrawTime / count
    lastShownImDrawTime = lastImDrawTime / count
    count = 0
    lastDrawTime = 0
    lastImDrawTime = 0
  end
end

local createSnapshot = false

local isDown = {}
function snap.mousepressed(x, y, button)
  isDown[button] = true

  local mx, my = snap.mouse.getPosition()
  mx = mx - viewport.offset.x
  my = my - viewport.offset.y
  mx = mx / viewport.size.x
  my = my / viewport.size.y

  if button == 1 and mx >= 0 and mx <= 1 and my >= 0 and my <= 1 then
    snap.renderer.pickObject(mx, my)
  end
end

function snap.mousereleased(x, y, button)
  isDown[button] = false
end

function snap.keypressed(key)
  isDown[key] = true
  if key == "f5" then
    snap.renderer.reloadShaders()
  elseif key == "f6" then
    createSnapshot = true
  end
end

function snap.keyreleased(key)
  isDown[key] = false
end

function snap.mousemoved(x, y, dx, dy)
  if not isDown[3] then
    return
  end

  local userdata = camera:getUserdata()

  if not userdata then
    userdata = camera:setUserdata({
      rotation = vec3()
    })
  end

  local rotation = userdata.rotation

  rotation.y = rotation.y - dx * 0.0015
  rotation.x = rotation.x - dy * 0.0015

  rotation.x = math.max(math.min(rotation.x, math.pi / 2), -math.pi / 2)
  camera:setRotation(snap.math.eulerToQuaternion(rotation:get()))
end

function update(dt)
  local speed = dt * 10

  if (isDown["a"]) then
    local leftX, leftY, leftZ = camera:getRight()
    leftX, leftY, leftZ = -leftX, -leftY, -leftZ
    local x, y, z = camera:getPosition()
    camera:setPosition(x + leftX * speed, y + leftY * speed, z + leftZ * speed)
  end

  if (isDown["d"]) then
    local rightX, rightY, rightZ = camera:getRight()
    local x, y, z = camera:getPosition()
    camera:setPosition(x + rightX * speed, y + rightY * speed, z + rightZ * speed)
  end

  if (isDown["w"]) then
    local forwardX, forwardY, forwardZ = camera:getForward()
    local x, y, z = camera:getPosition()
    camera:setPosition(x + forwardX * speed, y + forwardY * speed, z + forwardZ * speed)
  end

  if (isDown["s"]) then
    local backX, backY, backZ = camera:getForward()
    backX, backY, backZ = -backX, -backY, -backZ
    local x, y, z = camera:getPosition()
    camera:setPosition(x + backX * speed, y + backY * speed, z + backZ * speed)
  end

  if (isDown["space"]) then
    local upX, upY, upZ = camera:getUp()
    local x, y, z = camera:getPosition()
    camera:setPosition(x + upX * speed, y + upY * speed, z + upZ * speed)
  end

  if (isDown["lctrl"]) then
    local downX, downY, downZ = camera:getUp()
    downX, downY, downZ = -downX, -downY, -downZ
    local x, y, z = camera:getPosition()
    camera:setPosition(x + downX * speed, y + downY * speed, z + downZ * speed)
  end
end

local deltaTimestamp = snap.timer.getTime()
local startupSequence = {
  function()
    snap.graphics.setDefaultFilter("linear", "linear", 16)

    local texture = snap.graphics.newTexture("src/Assets/skybox.hdr", { sampler = true, mipmaps = "init" })
    texture:setFilter("linear", "linear", "linear")
    texture:setWrap("repeat", "repeat", "repeat")

    local env = scene:newEnvironment("Test environment", texture)
    scene:setEnvironment(env)
    snap.scene.loadModel(scene, "Assets/Terrain/sponza.glb")
    -- snap.scene.loadModel(scene, "Assets/Terrain/Bistro/bistro.gltf")
  end,
  function()
  end
}

local frameIndex = 0

while true do
  if not (canStartChannel:demand(1)) then
    print("Render thread received stop signal")
    break
  end

  frameIndex = frameIndex + 1

  local delta = snap.timer.getTime() - deltaTimestamp
  deltaTimestamp = snap.timer.getTime()
  delta = math.max(math.min(delta, 0.1), 0.0001)

  if createSnapshot then
    print("Requesting snapshot creation")
  end

  snap.graphics.acquireGraphics(nil, nil, "Graphics", createSnapshot)

  createSnapshot = false

  local data = events:pop()
  while data do
    local event = buffer.decode(data)
    if snap[event[1]] then
      snap[event[1]](unpack(event, 2))
    end

    data = events:pop()
  end

  update(delta)

  -- assert(frameIndex < 4)

  snap.graphics.setWindingOrder("cw")

  if startupSequence[frameIndex] then
    startupSequence[frameIndex]()
  end

  snap.gui.newFrame(delta)

  draw()

  local commands, newSnapshot = snap.graphics.submitGraphics()
  if newSnapshot then
    snapshot = newSnapshot
  end

  commandBufferChannel:push(commands)
end

collectgarbage("collect")
collectgarbage("collect")
