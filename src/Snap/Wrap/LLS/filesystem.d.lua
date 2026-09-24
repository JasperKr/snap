---@meta Filesystem

error("Do not require this file")

snap.filesystem = {}

--[[
auto Wrap_Read(lua_State *state) -> int;
auto Wrap_Append(lua_State *state) -> int;
auto Wrap_Write(lua_State *state) -> int;
auto Wrap_FileExists(lua_State *state) -> int;
auto Wrap_GetFileInfo(lua_State *state) -> int;
auto Wrap_Mount(lua_State *state) -> int;
auto Wrap_Unmount(lua_State *state) -> int;
auto Wrap_GetRealPath(lua_State *state) -> int;
auto Wrap_ListFiles(lua_State *state) -> int;

auto Wrap_GetSaveDirectory(lua_State *state) -> int;
auto Wrap_GetSourceDirectory(lua_State *state) -> int;
auto Wrap_GetSourceBaseDirectory(lua_State *state) -> int;
]]

--- Reads the contents of a file
---@overload fun(format: "text", path: string): string
---@overload fun(format: "binary", path: string): snap.Bytedata
---@param path string path to the file
---@return string content file contents
function snap.filesystem.read(path) end

--- Appends data to a file
---@overload fun(path: string, data: snap.Bytedata): boolean success
---@param path string path to the file
---@param data string data to append
---@return boolean success whether the operation was successful
function snap.filesystem.append(path, data) end

--- Writes data to a file
--- @overload fun(path: string, data: snap.Bytedata): boolean success
--- @param path string path to the file
--- @param data string data to write
--- @return boolean success whether the operation was successful
function snap.filesystem.write(path, data) end

--- Checks if a file exists
---@param path string path to the file
---@return boolean exists whether the file exists
function snap.filesystem.fileExists(path) end

---@alias snap.FileType "file" | "directory" | "symlink" | "other"

---@alias snap.FileInfo { size: integer, modtime: integer, createtime: integer, accesstime: integer, type: snap.FileType }

--- Gets information about a file
--- @param path string path to the file
--- @return snap.FileInfo info file information
function snap.filesystem.getFileInfo(path) end

--- Mounts a filesystem path
---@param path string path to mount
---@param mountpoint string mountpoint to mount at
---@param append boolean whether to append to the mountpoint
---@return boolean success whether the operation was successful
function snap.filesystem.mount(path, mountpoint, append) end

--- Unmounts a filesystem path
---@param mountpoint string mountpoint to unmount
---@return boolean success whether the operation was successful
function snap.filesystem.unmount(mountpoint) end

--- Gets the real path of a virtual path
---@param path string virtual path
---@return string realpath real path
function snap.filesystem.getRealPath(path) end

--- Lists files in a directory
---@overload fun(path: string, out: string[]): string[]
---@param path string directory path
---@return string[] files list of files
function snap.filesystem.listFiles(path) end

--- Gets the save directory path
---@return string path save directory path
function snap.filesystem.getSaveDirectory() end

--- Gets the source directory path
---@return string path source directory path
function snap.filesystem.getSourceDirectory() end

--- Gets the source base directory path
---@return string path source base directory path
function snap.filesystem.getSourceBaseDirectory() end
