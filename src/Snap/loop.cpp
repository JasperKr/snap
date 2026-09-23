#include "loop.hpp"
#include "Editor/gui.hpp"
#include "Graphics/Buffers/uniform.hpp"
#include "Graphics/bvh.hpp"
#include "Graphics/deviceInfo.hpp"
#include "Graphics/graphics.hpp"
#include "Graphics/render.hpp"
#include "Graphics/renderState.hpp"
#include "Graphics/shader.hpp"
#include "Graphics/snapshot.hpp"
#include "Graphics/texture.hpp"
#include "Modules/config.hpp"
#include "Modules/console.hpp"
#include "Modules/error.hpp"
#include "Modules/filesystem.hpp"
#include "Modules/thread.hpp"
#include "Modules/window.hpp"
#include "Wrap/wrap_engine.hpp"
#include <filesystem>
#include <lua.hpp>
#include <public/tracy/Tracy.hpp>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Modules/event.hpp"
#include "Wrap/reference.hpp"
#include "Wrap/wrap.hpp"

#ifdef BUNDLE_ENGINE
#include "renderer.hpp"
#endif

constexpr auto defaultRunFunction = R"lua(
function snap.run()

  if snap.load then
    snap.load()
  end
  local events = {}

  return function()
    snap.event.pull()

    local _, count = snap.event.pop(events)
    while count do
      local name = events[1]

      if name == "quit" then
        if snap.quit then
          return snap.quit() or 0
        end
        return 0
      end

      if snap[name] then
        snap[name](unpack(events, 2, count))
      end
      if snap.any then
        snap.any(unpack(events, 1, count))
      end

      _, count = snap.event.pop(events)
    end

    snap.timer.step()

    local dt = snap.timer.getDelta()

    if snap.update then
      snap.update(dt)
    end

    if snap.graphics then
      local commandBuffers

      if snap.draw then
        commandBuffers = snap.draw()
      end

      if commandBuffers then
        snap.graphics.present(commandBuffers)
      end
    end
  end
end
)lua";

// NOLINTNEXTLINE
static LuaWrap::LuaRef runCallback;

auto LoadLua(lua_State *state, const std::vector<std::string> &launchArgs)
    -> Error {
  // Load src/Engine/main.lua
  PrintDebug("Loading main Lua script...");
  if (launchArgs.empty()) {
    return Error::Create("No launch arguments provided for Lua script.");
  }

  lua_getglobal(state, "debug");
  lua_getfield(state, -1, "traceback");
  lua_remove(state, -2); // remove debug table from stack
  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 1); // remove non-function from stack
    return Error::Create("debug.traceback is not a function.");
  }

  lua_getglobal(state, "package");
  lua_getfield(state, -1, "path");
  std::string currentPath = lua_tostring(state, -1);
  lua_pop(state, 1); // remove original path
  std::string newPath =
      Path::Join(Filesystem::GetSourceDirectory(), std::string("?.lua;")) +
      currentPath;
  lua_pushstring(state, newPath.c_str());
  lua_setfield(state, -2, "path");
  lua_pop(state, 1); // remove package table

  auto luaLoadErr =
      luaL_loadfile(state, Path::Join(Filesystem::GetSourceDirectory(),
                                      std::string("main.lua"))
                               .c_str());

  if (luaLoadErr != LUA_OK) {
    if (lua_type(state, -1) == LUA_TSTRING) {
      std::string luaErrorMessage;
      if (lua_objlen(state, -1) == 0) {
        luaErrorMessage = "";
      } else {
        luaErrorMessage = lua_tostring(state, -1);
      }
      lua_pop(state, 1); // Remove error message from stack
      return Error::Create(luaErrorMessage);
    }

    lua_pop(state, 1); // Remove non-string error from stack
    return Error::Create("Failed to load main Lua script: Unknown error");
  }

  for (const auto &arg : launchArgs) {
    lua_pushstring(state, arg.c_str());
  }

  // Call the loaded chunk
  if (lua_pcall(state, static_cast<int>(launchArgs.size()), 0, 1) != LUA_OK) {
    if (lua_type(state, -1) != LUA_TSTRING) {
      lua_pop(state, 1); // Remove non-string error from stack
      return Error::Create("Failed to run main Lua script: Unknown error");
    }

    if (lua_objlen(state, -1) == 0) {
      lua_pop(state, 1); // Remove error message from stack
      return Error::Create("Unknown Lua error in main script.");
    }

    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Create("Failed to run main Lua script: " + luaErrorMessage);
  }

  // Get snap.run function
  lua_getglobal(state, "snap");
  lua_getfield(state, -1, "run");

  if (!lua_isfunction(state, -1)) {
    // If snap.run is not defined, load default
    PrintDebug("snap.run not found, loading default run function...");
    lua_pop(state, 2); // Remove non-function and snap table from stack
    if (luaL_dostring(state, defaultRunFunction) != LUA_OK) {
      std::string luaErrorMessage = lua_tostring(state, -1);
      lua_pop(state, 1); // Remove error message from stack
      return Error::Create("Failed to load default run function: " +
                           luaErrorMessage);
    }
    lua_getglobal(state, "snap");
    lua_getfield(state, -1, "run");
  }

  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 1); // Remove non-function from stack
    return Error::Create("snap.run is not a function.");
  }

  // Call snap.run to get the main loop function
  if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
    std::string luaErrorMessage = lua_tostring(state, -1);
    lua_pop(state, 1); // Remove error message from stack
    return Error::Create("Failed to call snap.run: " + luaErrorMessage);
  }

  if (!lua_isfunction(state, -1)) {
    lua_pop(state, 1); // Remove non-function from stack
    return Error::Create("snap.run did not return a function.");
  }

  PrintDebug("Main Lua script loaded successfully.");

  runCallback = LuaWrap::LuaRef::FromStack(state);

  return Error::Success();
}

auto RegisterAllLuaModules(lua_State *state) -> void {
  LuaWrap::RegisterModules(state);
  Engine::LuaWrap::RegisterModules(state);
}

#ifdef DEBUG_OBJECT_LIFETIMES
inline auto AtExit() -> void {
  if (!RefCounts.empty()) {
    PrintWarning("There are still {} live objects at shutdown!",
                 RefCounts.size());

    for (const auto &[ptr, info] : RefCounts) {
      PrintWarning(" - {} ({} references)", info.second, info.first.load());
    }
  } else {
    PrintInfo("All objects were properly released at shutdown.");
  }
}
#endif

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto MainLoop(const std::vector<std::string> &arguments) -> Error {
  Error::SetupTraceback();

  Window::WindowContext wcontext = {};
  Window::SetWindowContext(wcontext);

  lua_State *state = luaL_newstate();
  luaL_openlibs(state);

  RegisterAllLuaModules(state);

  if (arguments.size() == 0) {
    return Error::Create("No Lua script specified to run.");
  }

  const auto &currentPath = std::filesystem::current_path();

  auto sourceDirectory =
      Path::Sanitize(currentPath.string() + "/" + arguments[0]);
  sourceDirectory = Path::Directory(sourceDirectory);

  CHECK_ERR(Filesystem::Init("snap"));
  CHECK_ERR(Filesystem::SetSourceDirectory(sourceDirectory));
  CHECK_ERR(Filesystem::Mount(".", "/", true));
  CHECK_ERR(Filesystem::Mount(sourceDirectory, "/", true));

  auto config = CHECK_RES(Config::Configure(state, sourceDirectory));
  Filesystem::GetConfig().identity = config.Identity;

  CHECK_ERR(Filesystem::SetWriteDirectory(Filesystem::GetSaveDirectory()));

  Graphics::GraphicsContext context = {};

  PrintDebug("Initializing graphics...");

  CHECK_ERR(Graphics::Initialize(context, wcontext, config.deviceSettings));

  PrintDebug("Graphics initialized successfully.");

  Graphics::SetCurrentGraphicsContext(&context);

  CHECK_ERR(Graphics::semaphoreManager.Initialize(context));

  CHECK_ERR(Graphics::LoadShaderModule());

  CHECK_ERR(Graphics::InitializeRendering(context, wcontext));
  CHECK_ERR(Graphics::RenderState::Load(context));
  CHECK_ERR(Graphics::InitializeBVHModule(context));

  CHECK_ERR(InitializeUniformBufferModule(context));

  CHECK_ERR(Engine::Gui::LoadGUIState(state));

  CHECK_ERR(LoadLua(state, arguments));

#ifdef DEBUG_OBJECT_LIFETIMES
  std::atexit(AtExit);
#endif

  PrintDebug("Entering main loop...");

  lua_getglobal(state, "debug");
  lua_getfield(state, -1, "traceback");
  lua_remove(state, -2); // remove debug table
  auto tracebackIndex = lua_gettop(state);

  auto mainLoopResult = Error::Success();

  while (Event::MainLoopRunning) {
    runCallback.push();

    FrameMarkStart("Frame");
    if (lua_pcall(state, 0, 1, tracebackIndex) != LUA_OK) {
      std::string luaErrorMessage;
      if (lua_objlen(state, -1) == 0) {
        luaErrorMessage = "";
      } else {
        luaErrorMessage = lua_tostring(state, -1);
      }
      lua_pop(state, 1); // Remove error message from stack
      Event::MainLoopRunning = false;
      Event::ExitCode = 1;

      mainLoopResult = Error::Create(luaErrorMessage);
    }
    FrameMarkEnd("Frame");

    Graphics::Snapshot::Update();

    // returned value nil == continue, non-nil == exit with code
    if (lua_isnoneornil(state, -1)) {
      lua_pop(state, 1); // pop nil
    } else {
      int exitCode = static_cast<int>(lua_tointeger(state, -1));
      lua_pop(state, 1); // pop exit code
      PrintInfo("Exiting main loop with code " + std::to_string(exitCode));
      Event::ExitCode = exitCode;
      Event::MainLoopRunning = false;
    }
  }

  // Uses internal lua state so we must do this before closing lua
  runCallback.reset();

  // Force all deferred destructions to happen now
  Graphics::GetDeferredDestructionAllowed() = false;

#ifdef BUNDLE_ENGINE
  Engine::Renderer::RendererInstance.Deinitialize();
#endif

  Graphics::UnloadModule();

  Graphics::DeInitializeUniformBufferModule(context);

  PrintInfo("Closing Lua state...");
  lua_close(state);

  PrintInfo("Deinitializing threading module...");

  Threading::UnloadModule();

  PrintInfo("Unloading shader modules...");

  PrintInfo("Destroying graphics context...");

  PrintInfo("Texture memory: {} bytes",
            Graphics::Texture::TotalAllocatedMemory.load());
  PrintInfo("Buffer memory: {} bytes",
            Graphics::Buffer::TotalAllocatedMemory.load());

  Graphics::Deinitialize(context);

  PrintInfo("App shutdown complete.");

  return mainLoopResult;
}
