SnapEngine.keybindings.addAction("pick object", function()
  local mx, my = GetViewportRelativeMousePosition()
  if mx >= 0 and mx <= 1 and my >= 0 and my <= 1 then
    snap.editor.pickObject(mx, my)
  end
end)

SnapEngine.keybindings.addBinding({
  action = "pick object",
  rising = 1,
})

SnapEngine.keybindings.addAction("reload shaders", snap.renderer.reloadShaders)
SnapEngine.keybindings.addBinding({
  action = "reload shaders",
  falling = "f5",
})

SnapEngine.keybindings.addAction("create snapshot", CreateSnapshot)
SnapEngine.keybindings.addBinding({
  action = "create snapshot",
  falling = "f6",
})


SnapEngine.keybindings.addAction("move editor camera left", function()
  local speed = snap.timer.getDelta() * 10

  local leftX, leftY, leftZ = SnapEngine.editor.camera:getRight()
  leftX, leftY, leftZ = -leftX, -leftY, -leftZ
  local x, y, z = SnapEngine.editor.camera:getPosition()
  SnapEngine.editor.camera:setPosition(x + leftX * speed, y + leftY * speed, z + leftZ * speed)
end)

SnapEngine.keybindings.addAction("move editor camera right", function()
  local speed = snap.timer.getDelta() * 10

  local rightX, rightY, rightZ = SnapEngine.editor.camera:getRight()
  local x, y, z = SnapEngine.editor.camera:getPosition()
  SnapEngine.editor.camera:setPosition(x + rightX * speed, y + rightY * speed, z + rightZ * speed)
end)

SnapEngine.keybindings.addAction("move editor camera forward", function()
  local speed = snap.timer.getDelta() * 10

  local forwardX, forwardY, forwardZ = SnapEngine.editor.camera:getForward()
  local x, y, z = SnapEngine.editor.camera:getPosition()
  SnapEngine.editor.camera:setPosition(x + forwardX * speed, y + forwardY * speed, z + forwardZ * speed)
end)

SnapEngine.keybindings.addAction("move editor camera backward", function()
  local speed = snap.timer.getDelta() * 10

  local backX, backY, backZ = SnapEngine.editor.camera:getForward()
  backX, backY, backZ = -backX, -backY, -backZ
  local x, y, z = SnapEngine.editor.camera:getPosition()
  SnapEngine.editor.camera:setPosition(x + backX * speed, y + backY * speed, z + backZ * speed)
end)

SnapEngine.keybindings.addAction("move editor camera up", function()
  local speed = snap.timer.getDelta() * 10

  local upX, upY, upZ = SnapEngine.editor.camera:getUp()
  local x, y, z = SnapEngine.editor.camera:getPosition()
  SnapEngine.editor.camera:setPosition(x + upX * speed, y + upY * speed, z + upZ * speed)
end)

SnapEngine.keybindings.addAction("move editor camera down", function()
  local speed = snap.timer.getDelta() * 10

  local downX, downY, downZ = SnapEngine.editor.camera:getUp()
  downX, downY, downZ = -downX, -downY, -downZ
  local x, y, z = SnapEngine.editor.camera:getPosition()
  SnapEngine.editor.camera:setPosition(x + downX * speed, y + downY * speed, z + downZ * speed)
end)

SnapEngine.keybindings.addBinding({
  action = "move editor camera left",
  high = "a",
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera right",
  high = "d",
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera forward",
  high = "w",
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera backward",
  high = "s",
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera up",
  high = "space",
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera down",
  high = "lctrl",
})
