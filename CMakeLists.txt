cmake_minimum_required(VERSION 3.25)
project(snap LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_RTTI "Enable RTTI" OFF)
option(ENABLE_EXCEPTIONS "Enable exceptions" OFF)
option(BUNDLE_ENGINE "Bundle the engine into the framework, this will skip compiling the engine" ON)
option(DOUBLE_PRECISION "Use double precision for math types" OFF)

# Just globally enable / disable..
if (NOT ENABLE_RTTI)
    add_compile_options(-fno-rtti)
endif()

if (NOT ENABLE_EXCEPTIONS)
    add_compile_options(-fno-exceptions)
endif()

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/std:c++23preview>")
else()
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

find_package(TBB REQUIRED)
find_package(OpenMP REQUIRED)


# -------------------------------
# SDL3
# -------------------------------
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# -------------------------------
# Flecs
# -------------------------------

cmake_policy(PUSH)
cmake_policy(SET CMP0077 NEW)

set(FLECS_STATIC ON)
set(FLECS_SHARED OFF)
set(FLECS_PIC ON)
set(FLECS_TESTS OFF)
add_subdirectory(external/flecs EXCLUDE_FROM_ALL)

cmake_policy(POP)

add_subdirectory(src/Snap)

# -------------------------------
# Engine executable
# -------------------------------
add_executable(snap
    src/Snap/main.cpp
    src/Snap/loop.cpp
    $<TARGET_OBJECTS:snap_core>
)

if (DOUBLE_PRECISION)
    target_compile_definitions(snap
        PRIVATE
            DOUBLE_PRECISION
    )
endif()

if (BUNDLE_ENGINE)
target_include_directories(snap
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src/SnapEngine
)

add_subdirectory(src/SnapEngine)

# Link libraries
target_link_libraries(snap
    PRIVATE
        snap_core
        SnapEngine
        TBB::tbb
        OpenMP::OpenMP_CXX
)
else()
target_link_libraries(snap
    PRIVATE
        snap_core
        TBB::tbb
        OpenMP::OpenMP_CXX
)
endif()


target_link_options(snap PRIVATE
    $<$<CONFIG:Debug>:-rdynamic>
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
    $<$<CONFIG:Debug>:-g>
)

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
        -Wno-gnu-statement-expression-from-macro-expansion
    )

elseif (MSVC)
    add_compile_options(
        /W4 /WX /permissive-
    )
endif()

if (BUNDLE_ENGINE)
  target_compile_definitions(snap
      PRIVATE
          BUNDLE_ENGINE
  )

  add_executable(lua_stub_gen
      src/Snap/generateBindings.cpp
      $<TARGET_OBJECTS:snap_core>
      $<TARGET_OBJECTS:SnapEngine>
  )

  target_compile_definitions(lua_stub_gen
    PRIVATE
      LUA_STUB_GEN
  )

  target_link_libraries(lua_stub_gen
      PRIVATE
          snap_core
          SnapEngine
  )
else()
  target_compile_definitions(snap
      PRIVATE
          NO_ENGINE
  )

  add_executable(lua_stub_gen
      src/Snap/generateBindings.cpp
      $<TARGET_OBJECTS:snap_core>
  )

  target_compile_definitions(lua_stub_gen
    PRIVATE
      LUA_STUB_GEN
  )

  target_link_libraries(lua_stub_gen
      PRIVATE
          snap_core
  )
endif()

add_custom_target(generate_bindings
    COMMAND lua_stub_gen
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating Lua bindings..."
)

