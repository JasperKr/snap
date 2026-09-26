---@meta Imgui

error("Do not require this file")

snap.gui = {}

function snap.gui.newFrame() end

function snap.gui.endFrame() end

function snap.gui.draw() end

function snap.gui.mousePressed(x, y, button) end

function snap.gui.mouseReleased(x, y, button) end

function snap.gui.mouseMoved(x, y) end

function snap.gui.mouseWheelMoved() end

function snap.gui.keyPressed(key) end

function snap.gui.keyReleased(key) end

function snap.gui.textInput(text) end

function snap.gui.getContextPtr() end

function snap.gui.getFontAtlasPtr() end

function snap.gui.shutdown() end

function snap.gui.applyDefaultStyle() end
