cmake_minimum_required(VERSION 3.25)
project(snap LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/std:c++23preview>")
else()
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

# -------------------------------
# Find Vulkan first
# -------------------------------
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found! Make sure VULKAN_SDK is set.")
endif()

# -------------------------------
# ImGui
# -------------------------------
add_library(imgui SHARED IMPORTED)
set(CIMGUI_LIB "${CMAKE_SOURCE_DIR}/bin/cimgui/cimgui.so")
set_target_properties(imgui PROPERTIES
    IMPORTED_LOCATION "${CIMGUI_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/external/imgui"
)
target_link_libraries(imgui INTERFACE SDL3::SDL3 Vulkan::Vulkan)

# -------------------------------
# PhysFS
# -------------------------------

set(PHYSFS_ROOT "${CMAKE_SOURCE_DIR}/external/physfs")

set(PHYSFS_SOURCES
    ${PHYSFS_ROOT}/src/physfs.c
    ${PHYSFS_ROOT}/src/physfs_byteorder.c
    ${PHYSFS_ROOT}/src/physfs_unicode.c
    ${PHYSFS_ROOT}/src/physfs_archiver_dir.c
    ${PHYSFS_ROOT}/src/physfs_archiver_unpacked.c
    ${PHYSFS_ROOT}/src/physfs_archiver_grp.c
    ${PHYSFS_ROOT}/src/physfs_archiver_hog.c
    ${PHYSFS_ROOT}/src/physfs_archiver_7z.c
    ${PHYSFS_ROOT}/src/physfs_archiver_mvl.c
    ${PHYSFS_ROOT}/src/physfs_archiver_qpak.c
    ${PHYSFS_ROOT}/src/physfs_archiver_wad.c
    ${PHYSFS_ROOT}/src/physfs_archiver_zip.c
    ${PHYSFS_ROOT}/src/physfs_archiver_slb.c
    ${PHYSFS_ROOT}/src/physfs_archiver_iso9660.c
    ${PHYSFS_ROOT}/src/physfs_archiver_csm.c
    ${PHYSFS_ROOT}/src/physfs_archiver_vdf.c
    ${PHYSFS_ROOT}/src/physfs_archiver_lec3d.c
)

if (WIN32)
    message(STATUS "Configuring for Windows")
    list(APPEND PHYSFS_SOURCES
        ${PHYSFS_ROOT}/src/physfs_platform_windows.c
    )
elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Unix")
    list(APPEND PHYSFS_SOURCES
        ${PHYSFS_ROOT}/src/physfs_platform_unix.c
        ${PHYSFS_ROOT}/src/physfs_platform_posix.c
    )
elseif(APPLE)
    message(STATUS "Configuring for Apple")
    list(APPEND PHYSFS_SOURCES
        ${PHYSFS_ROOT}/src/physfs_platform_apple.c
    )
endif()

add_library(physfs STATIC ${PHYSFS_SOURCES})
target_include_directories(physfs
    SYSTEM PRIVATE ${PHYSFS_ROOT}/src/
)

target_compile_definitions(physfs PRIVATE _POSIX_C_SOURCE=200809L)

set(BINARIES_DIR "${CMAKE_SOURCE_DIR}/bin/")
set(LIBRARIES_DIR "${CMAKE_SOURCE_DIR}/lib/")

# -------------------------------
# LuaJIT
# -------------------------------

include(ExternalProject)

set(LUAJIT_SRC "${CMAKE_SOURCE_DIR}/external/luajit/")

if (WIN32)
    ExternalProject_Add(luajit
        SOURCE_DIR ${LUAJIT_SRC}/src
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            cmd /c "cd ${LUAJIT_SRC}/src && msvcbuild.bat amalg"
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
    )

    # Define imported target
    add_library(LuaJIT::LuaJIT SHARED IMPORTED)

    set_target_properties(LuaJIT::LuaJIT PROPERTIES
        IMPORTED_LOCATION     "${LUAJIT_SRC}/src/lua51.dll"
        IMPORTED_IMPLIB       "${LUAJIT_SRC}/src/lua51.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC}/src"
    )
elseif(UNIX)
    set(LUAJIT_LIB "${LUAJIT_SRC}/src/libluajit.so")

    ExternalProject_Add(luajit
        SOURCE_DIR ${LUAJIT_SRC}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS ${LUAJIT_LIB}
    )

    add_library(LuaJIT::LuaJIT SHARED IMPORTED)

    set_target_properties(LuaJIT::LuaJIT PROPERTIES
        IMPORTED_LOCATION "${LUAJIT_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC}/src"
    )
endif()


# Ensure snap depends on LuaJIT build
add_dependencies(LuaJIT::LuaJIT luajit)

# -------------------------------
# FastGLTF
# -------------------------------

set(fastGLTFSources
    ${INCLUDE_DIR}/fastgltf/src/fastgltf.cpp
    ${INCLUDE_DIR}/fastgltf/src/base64.cpp
    ${INCLUDE_DIR}/fastgltf/src/io.cpp
)

add_library(fastgltf STATIC ${fastGLTFSources})

target_include_directories(fastgltf
    PUBLIC
        ${INCLUDE_DIR}/fastgltf/include
)

add_library(simdjson STATIC
    ${INCLUDE_DIR}/simdjson/simdjson.cpp
)

target_include_directories(simdjson
    PUBLIC ${INCLUDE_DIR}/simdjson
)

target_link_libraries(fastgltf
    PUBLIC simdjson
)

# -------------------------------
# Tracy
# -------------------------------

find_package(Threads REQUIRED)
add_library(tracy STATIC ${CMAKE_SOURCE_DIR}/external/tracy/public/TracyClient.cpp)

target_include_directories(tracy
    SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/external/tracy
)

# target_compile_definitions(tracy
    # PUBLIC TRACY_ENABLE
# )

target_link_libraries(tracy
    PUBLIC Threads::Threads
)


# -------------------------------
# Slang
# -------------------------------

set(SLANG_BIN_DIR "${BINARIES_DIR}/slang/")
set(SLANG_LIB_DIR "${LIBRARIES_DIR}/slang/")

if(WIN32)
    set(SLANG_LIBS
        ${SLANG_LIB_DIR}/slang.lib
        ${SLANG_LIB_DIR}/gfx.lib
        ${SLANG_LIB_DIR}/slang-compiler.lib
        ${SLANG_LIB_DIR}/slang-rt.lib
        )
elseif(UNIX)
    # set(SLANG_LIBS "${SLANG_LIB_DIR}/libgfx.so.0.2025.23.1")
    # Find all .so files in the directory
    file(GLOB SO_FILES "${SLANG_LIB_DIR}/*.so*")

    # Add them to the variable
    set(SLANG_LIBS ${SO_FILES})

endif()

# -------------------------------
# Engine core (shared object library)
# -------------------------------
add_library(snap_core OBJECT
# Graphics module
    Graphics/graphics.cpp
    Graphics/texture.cpp
    Graphics/buffer.cpp
    Graphics/render.cpp
    Graphics/shader.cpp
    Graphics/sampler.cpp
    Graphics/renderState.cpp
    Graphics/reflect.cpp
    Graphics/resource.cpp
    Graphics/mesh.cpp
    Graphics/draw.cpp
    Graphics/renderThread.cpp
    Graphics/semaphoreManager.cpp
    Graphics/bufferformat.cpp
    Graphics/swapchainManager.cpp
    Graphics/uniformWriter.cpp
    Graphics/format.cpp
    Graphics/snapshot.cpp
    Graphics/blendmode.cpp
    Graphics/allocations.cpp
    Graphics/bvh.cpp
    Graphics/shaderCache.cpp
    Graphics/vertexformat.cpp
    Graphics/vkAccessHelpers.cpp
    Graphics/vkPipelineHelpers.cpp

# Frame Graph
    Graphics/FrameGraph/framegraph.cpp
    Graphics/FrameGraph/commands.cpp
    Graphics/FrameGraph/resourceUsage.cpp
    Graphics/FrameGraph/descriptorCache.cpp
    Graphics/FrameGraph/descriptorState.cpp
    Graphics/FrameGraph/pipelineCache.cpp
    Graphics/FrameGraph/pipelineState.cpp
    Graphics/FrameGraph/dynamicRendering.cpp
    Graphics/FrameGraph/recordingState.cpp

# Graphics Buffers
    Graphics/Buffers/push.cpp
    Graphics/Buffers/uniform.cpp
    Graphics/Buffers/structured.cpp

# Engine Modules
    Modules/filesystem.cpp
    Modules/timer.cpp
    Modules/errorhandler.cpp
    Modules/config.cpp
    Modules/event.cpp
    Modules/object.cpp
    Modules/imageData.cpp
    Modules/compressedImageData.cpp
    Modules/Peripherals/mouse.cpp
    Modules/Peripherals/keyboard.cpp
    Modules/thread.cpp
    Modules/channel.cpp
    Modules/error.cpp
    Modules/window.cpp
    Modules/reflectBindings.cpp
    Modules/image.cpp

# Math Modules
    Modules/Math/vector.cpp
    Modules/Math/matrix.cpp
    Modules/Math/quaternion.cpp
    Modules/Math/math.cpp
    Modules/Math/ray.cpp

# Helpers
    Modules/Helpers/hasher.cpp
    Modules/Helpers/utils.cpp

# Wrappers
    Wrap/Graphics/wrap_graphics.cpp
    Wrap/Graphics/wrap_texture.cpp
    Wrap/Graphics/wrap_rendertarget.cpp
    Wrap/Graphics/wrap_mesh.cpp
    Wrap/Graphics/wrap_shader.cpp
    Wrap/Graphics/wrap_buffer.cpp
    Wrap/Graphics/wrap_format.cpp
    Wrap/Graphics/wrap_snapshot.cpp

    Wrap/Modules/wrap_event.cpp
    Wrap/Modules/wrap_timer.cpp
    Wrap/Modules/wrap_data.cpp
    Wrap/Modules/wrap_bytedata.cpp
    Wrap/Modules/wrap_imagedata.cpp
    Wrap/Modules/wrap_filesystem.cpp
    Wrap/Modules/wrap_keyboard.cpp
    Wrap/Modules/wrap_mouse.cpp
    Wrap/Modules/wrap_thread.cpp
    Wrap/Modules/wrap_channel.cpp
    Wrap/Modules/wrap_window.cpp
    Wrap/Modules/wrap_math.cpp
    Wrap/wrap.cpp
    Wrap/lua_data.cpp

# Libraries
    Libraries/vma.cpp
    Libraries/load_stb.cpp
    Libraries/load_volk.cpp
)

target_include_directories(snap_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/Snap
)
target_include_directories(snap_core SYSTEM PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external/physfs/src
        ${Vulkan_INCLUDE_DIRS}
)

target_compile_definitions(snap_core
    PUBLIC VK_NO_PROTOTYPES
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_ENABLE_FREETYPE
    PRIVATE
        IMGUI_IMPL_VULKAN_NO_PROTOTYPES
        IMGUI_IMPL_VULKAN_USE_LOADER
)

target_link_libraries(snap_core
    PUBLIC
        imgui
        SDL3::SDL3
        Vulkan::Vulkan
        physfs
        ${SHADERC_LIB}
        ${Vulkan_LIBRARIES}
        LuaJIT::LuaJIT
        ${SLANG_LIBS}
        fastgltf
        tracy
        backtrace
        dl
        unwind
        unwind-x86_64
        flecs::flecs_static

)

# Copy LuaJIT DLL after build
if (WIN32)
    add_custom_command(TARGET snap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUAJIT_SRC}/src/lua51.dll"
            $<TARGET_FILE_DIR:snap>)
endif()

# -------------------------------
# Windows: copy SDL3.dll after build
# -------------------------------
if (WIN32)
    add_custom_command(TARGET snap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:snap>
    )
endif()

# -------------------------------
# Compiler warnings & options
# -------------------------------
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wdouble-promotion
        -Werror
    )

elseif (MSVC)
    add_compile_options(
        /W4 /WX /permissive-
    )
endif()