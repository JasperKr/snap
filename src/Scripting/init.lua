-- In this file we initialize all required modules.

SnapEngine = {
  helpers = {},
  graphics = {},
  math = {},
  internal = {},
}

Imgui = require("Libraries.cimgui.init")
require("Modules.vec")
require("Modules.quaternions")
require("Modules.matrices")
require("Modules.math")
require("Modules.helpers")
require("Modules.pooledObjects")
require("Modules.stringHelpers")
require("Modules.tables")
require("Modules.keybindings")

table.clear = require("table.clear")
table.new = require("table.new")

require("Editor.Gui.imguiHelper")
require("Editor.Gui.gui")
require("Editor.Gui.graph")
require("Libraries.bitser")
