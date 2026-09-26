---@meta Editor

error("Do not require this file")

snap.editor = {}

function snap.editor.pickObject(x, y) end

---@param camera snap.Camera
function snap.editor.setCamera(camera) end

function snap.editor.setRelativeMousePosition(x, y) end

function snap.editor.startTranslating() end

function snap.editor.startRotating() end

function snap.editor.startScaling() end

---@param axis snap.TransformAxis
function snap.editor.setTransformAxis(axis) end

function snap.editor.applyTransform() end
