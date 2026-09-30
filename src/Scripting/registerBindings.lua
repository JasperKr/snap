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

-- SnapEngine.keybindings.addAction("create snapshot", CreateSnapshot)
-- SnapEngine.keybindings.addBinding({
--   action = "create snapshot",
--   falling = "f6",
-- })

-- MARK: Editor camera

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
  high = { "a", 3 },
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera right",
  high = { "d", 3 },
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera forward",
  high = { "w", 3 },
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera backward",
  high = { "s", 3 },
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera up",
  high = { "space", 3 },
})

SnapEngine.keybindings.addBinding({
  action = "move editor camera down",
  high = { "lctrl", 3 },
})

-- MARK: Gizmo

SnapEngine.keybindings.addAction("Gizmo - Translate", snap.editor.startTranslating)
SnapEngine.keybindings.addAction("Gizmo - Rotate", snap.editor.startRotating)
SnapEngine.keybindings.addAction("Gizmo - Scale", snap.editor.startScaling)

SnapEngine.keybindings.addAction("Gizmo - x-axis", snap.editor.setTransformAxis, "x")
SnapEngine.keybindings.addAction("Gizmo - y-axis", snap.editor.setTransformAxis, "y")
SnapEngine.keybindings.addAction("Gizmo - z-axis", snap.editor.setTransformAxis, "z")

SnapEngine.keybindings.addAction("Gizmo - yz-plane", snap.editor.setTransformAxis, "yz")
SnapEngine.keybindings.addAction("Gizmo - xz-plane", snap.editor.setTransformAxis, "xz")
SnapEngine.keybindings.addAction("Gizmo - xy-plane", snap.editor.setTransformAxis, "xy")

SnapEngine.keybindings.addAction("Gizmo - xyz freeform", snap.editor.setTransformAxis, "xyz")

SnapEngine.keybindings.addBinding({
  action = "Gizmo - Translate",
  rising = "g",
  low = 3
})

SnapEngine.keybindings.addBinding({
  action = "Gizmo - Rotate",
  rising = "r",
  low = 3
})

SnapEngine.keybindings.addBinding({
  action = "Gizmo - Scale",
  rising = "s",
  low = 3
})

SnapEngine.keybindings.addBinding(
  {
    action = "Gizmo - x-axis",
    rising = "x",
  }
)

SnapEngine.keybindings.addBinding(
  {
    action = "Gizmo - y-axis",
    rising = "y",
  }
)

SnapEngine.keybindings.addBinding(
  {
    action = "Gizmo - z-axis",
    rising = "z",
  }
)

SnapEngine.keybindings.addBinding(
  {
    action = "Gizmo - yz-plane",
    rising = "x",
    high = "lshift"
  }
)

SnapEngine.keybindings.addBinding(
  {
    action = "Gizmo - xz-plane",
    rising = "y",
    high = "lshift"
  }
)

SnapEngine.keybindings.addBinding(
  {
    action = "Gizmo - xy-plane",
    rising = "z",
    high = "lshift"
  }
)

-- SnapEngine.keybindings.addBinding(
--   {
--     action = "Gizmo - xyz freeform",
--     rising = "xyz",
--   }
-- )
