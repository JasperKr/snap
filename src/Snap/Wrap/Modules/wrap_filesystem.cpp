#include "wrap_filesystem.hpp"
#include "Modules/bytedata.hpp"
#include "Wrap/wrap.hpp"
#include <cstdint>
#include <cstring>
#include <lua.hpp>
#include <string>

#include "Modules/filesystem.hpp"

namespace Wrap::Filesystem {

// path: string, [length: integer (bytes)] (text)
// type: string ("binary" | "text"), path: string, [length: integer (bytes)]
auto Wrap_Read(lua_State *state) -> int {
  std::string path;
  bool isBinary = false;
  bool hasTypeArg = false;

  if (lua_type(state, 2) == LUA_TSTRING) {
    hasTypeArg = true;
    isBinary = strcmp(luaL_checkstring(state, 1), "binary") == 0;
    if (!isBinary && strcmp(luaL_checkstring(state, 1), "text") != 0) {
      return luaL_error(state, "Invalid read type, expected 'binary' or 'text'");
    }
    path = lua_tostring(state, 2);
  } else {
    path = lua_tostring(state, 1);
  }

  int64_t readLength = INT64_MAX;

  if (hasTypeArg) {
    if (lua_gettop(state) >= 3 && lua_type(state, 3) == LUA_TNUMBER) {
      readLength = static_cast<int64_t>(luaL_checkinteger(state, 3));
    }
  } else {
    if (lua_gettop(state) >= 2 && lua_type(state, 2) == LUA_TNUMBER) {
      readLength = static_cast<int64_t>(luaL_checkinteger(state, 2));
    }
  }

  if (isBinary) {
    auto result = ::Filesystem::ReadFile(path, readLength);

    if (Error::IsError(result)) {
      return luaL_error(state, "Failed to read file: %s", result.error().message.c_str());
    }

    const auto &data = result.value();
    auto bytedata = Ref<Data::ByteData>::Make(data.size());
    std::memcpy(bytedata->GetData(), data.data(), data.size());

    LuaWrap::PushObject(state, Data::ByteData::GetType(), bytedata.get());

    return 1;
  }

  auto result = ::Filesystem::ReadTextFile(path, readLength);

  if (Error::IsError(result)) {
    return luaL_error(state, "Failed to read text file: %s", result.error().message.c_str());
  }

  lua_pushstring(state, result.value().c_str());
  return 1;
}

// path: string, data: Bytedata | string
auto Wrap_Append(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);

  if (LuaWrap::IsType<Data::ByteData>(state, 2)) {
    auto bytedata = LUA_CK_NULL(LuaWrap::ObjectFromLua<Data::ByteData>(state, 2));

    LUA_CK_ERR(::Filesystem::AppendFile(path, std::span<const uint8_t>(bytedata->GetData(), bytedata->GetSize())));

    lua_pushboolean(state, 1);
    return 1;
  }

  if (lua_type(state, 2) == LUA_TSTRING) {
    size_t length = 0;
    const char *data = luaL_checklstring(state, 2, &length);

    LUA_CK_ERR(::Filesystem::AppendFile(path, std::string_view(data, length)));

    lua_pushboolean(state, 1);
    return 1;
  }

  return luaL_error(state, "Invalid data type for append, expected Bytedata or string");
}

// path: string, data: Bytedata | string
auto Wrap_Write(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);

  if (LuaWrap::IsType<Data::ByteData>(state, 2)) {
    auto bytedata = LuaWrap::ObjectFromLua<Data::ByteData>(state, 2);

    LUA_CK_ERR(::Filesystem::WriteFile(path, std::span<const uint8_t>(bytedata->GetData(), bytedata->GetSize())));

    lua_pushboolean(state, 1);
    return 1;
  }

  if (lua_type(state, 2) == LUA_TSTRING) {
    size_t length = 0;
    const char *data = luaL_checklstring(state, 2, &length);

    LUA_CK_ERR(::Filesystem::WriteFile(path, std::string_view(data, length)));

    lua_pushboolean(state, 1);
    return 1;
  }

  return luaL_error(state, "Invalid data type for write, expected Bytedata or string");
}

// path: string
auto Wrap_FileExists(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);
  bool exists = ::Filesystem::FileExists(path);
  lua_pushboolean(state, exists ? 1 : 0);
  return 1;
}

// path: string
auto Wrap_GetFileInfo(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);
  PHYSFS_Stat stat = ::Filesystem::GetFileInfo(path);

  lua_newtable(state);

  lua_pushstring(state, "size");
  lua_pushinteger(state, static_cast<lua_Integer>(stat.filesize));
  lua_settable(state, -3);

  lua_pushstring(state, "modtime");
  lua_pushinteger(state, static_cast<lua_Integer>(stat.modtime));
  lua_settable(state, -3);

  lua_pushstring(state, "createtime");
  lua_pushinteger(state, static_cast<lua_Integer>(stat.createtime));
  lua_settable(state, -3);

  lua_pushstring(state, "accesstime");
  lua_pushinteger(state, static_cast<lua_Integer>(stat.accesstime));
  lua_settable(state, -3);

  lua_pushstring(state, "filetype");
  // lua_pushinteger(state, static_cast<lua_Integer>(stat.filetype));

  switch (stat.filetype) {
  case PHYSFS_FILETYPE_REGULAR:
    lua_pushstring(state, "file");
    break;
  case PHYSFS_FILETYPE_DIRECTORY:
    lua_pushstring(state, "directory");
    break;
  case PHYSFS_FILETYPE_SYMLINK:
    lua_pushstring(state, "symlink");
    break;
  case PHYSFS_FILETYPE_OTHER:
    lua_pushstring(state, "other");
    break;
  default:
    lua_pushstring(state, "unknown");
    break;
  }

  lua_settable(state, -3);

  return 1;
}

// path: string, mountPoint: string, [appendToPath: boolean]
// returns: boolean success
auto Wrap_Mount(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);
  std::string mountPoint = luaL_checkstring(state, 2);
  bool appendToPath = false;

  if (lua_gettop(state) >= 3 && lua_type(state, 3) == LUA_TBOOLEAN) {
    appendToPath = lua_toboolean(state, 3) != 0;
  }

  auto error = ::Filesystem::Mount(path, mountPoint, appendToPath);

  lua_pushboolean(state, Error::IsSuccess(error) ? 1 : 0);
  return 1;
}

// path: string
// returns: boolean success
auto Wrap_Unmount(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);

  auto error = ::Filesystem::Unmount(path);

  lua_pushboolean(state, Error::IsSuccess(error) ? 1 : 0);
  return 1;
}

// path: string
// returns: string realPath | nil
auto Wrap_GetRealPath(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);

  auto result = ::Filesystem::GetRealPath(path);

  if (Error::IsError(result)) {
    lua_pushnil(state);
    return 1;
  }

  lua_pushstring(state, result.value().c_str());
  return 1;
}

// path: string, [out-var: table]
// returns: table files, count
auto Wrap_ListFiles(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);

  auto result = ::Filesystem::ListFiles(path);

  // Create table if not provided
  if (lua_gettop(state) == 1 || !lua_istable(state, 2)) {
    lua_newtable(state);
  }

  if (Error::IsError(result)) {
    lua_pushinteger(state, 0);
    return 2;
  }

  const auto &files = result.value();

  for (size_t i = 0; i < files.size(); ++i) {
    lua_pushinteger(state, static_cast<lua_Integer>(i + 1));
    lua_pushstring(state, files[i].c_str());
    lua_settable(state, -3);
  }

  lua_pushinteger(state, static_cast<lua_Integer>(files.size()));

  return 1;
}

// creates a directory at the given path
auto Wrap_CreateDirectory(lua_State *state) -> int {
  std::string path = luaL_checkstring(state, 1);
  LUA_CK_ERR(::Filesystem::CreateDirectory(path));
  return 0;
}

// returns: string save directory
auto Wrap_GetSaveDirectory(lua_State *state) -> int {
  std::string saveDir = ::Filesystem::GetSaveDirectory();
  lua_pushstring(state, saveDir.c_str());
  return 1;
}

// returns: string source directory
auto Wrap_GetSourceDirectory(lua_State *state) -> int {
  std::string sourceDir = ::Filesystem::GetSourceDirectory();
  lua_pushstring(state, sourceDir.c_str());
  return 1;
}

// returns: string source base directory
auto Wrap_GetSourceBaseDirectory(lua_State *state) -> int {
  std::string sourceBaseDir = ::Filesystem::GetSourceBaseDirectory();
  lua_pushstring(state, sourceBaseDir.c_str());
  return 1;
}

} // namespace Wrap::Filesystem