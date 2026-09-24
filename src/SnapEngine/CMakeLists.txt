# MikkTSpace

set(MikkTSpace_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/external/MikkTSpace)

message(STATUS "SnapEngine include: ${PROJECT_SOURCE_DIR}/src/Snap")

add_library(SnapEngine
  material.cpp
  gltfLoader.cpp
  renderer.cpp
  style.cpp

  Editor/gui.cpp
  Editor/lineDrawer.cpp
  Editor/primitiveDrawer.cpp
  Editor/editor.cpp

  Scene/scene.cpp
  Scene/project.cpp
  Scene/transform.cpp
  Scene/userdata.cpp
  Scene/frustum.cpp
  Scene/camera.cpp
  Scene/displayName.cpp
  Scene/cameraMatrices.cpp
  Scene/environment.cpp

  Scene/Geometry/shape.cpp
  Scene/Geometry/levelOfDetail.cpp
  Scene/Geometry/boundingBox.cpp
  Scene/Geometry/geometry.cpp
  Scene/Geometry/model.cpp

  Scene/Lights/light.cpp
  Scene/Lights/directionalLight.cpp
  Scene/Lights/pointLight.cpp
  Scene/Lights/spotLight.cpp
  Scene/Lights/rectangleLight.cpp
  Scene/Lights/sphereLight.cpp

  # Renderer
  Renderer/rendertargetManager.cpp
  Renderer/lightProbe.cpp
  Renderer/prefilterManager.cpp
  Renderer/bloomManager.cpp
  Renderer/shaderManager.cpp

  # Wrappers
  Wrap/wrap_scene.cpp
  Wrap/wrap_renderer.cpp
  Wrap/wrap_imgui.cpp
  Wrap/wrap_engine.cpp
  Wrap/wrap_editor.cpp

  # MikkTSpace
  ${MikkTSpace_INCLUDE_DIR}/mikktspace.c
)

target_include_directories(SnapEngine
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/Snap
    ${PROJECT_SOURCE_DIR}/src/SnapEngine
)

message(STATUS "SnapEngine include: ${PROJECT_SOURCE_DIR}/src/Snap")

add_subdirectory(
  ${CMAKE_SOURCE_DIR}/include/meshoptimizer EXCLUDE_FROM_ALL
  ${CMAKE_BINARY_DIR}/include/meshoptimizer
)

target_link_libraries(SnapEngine
  PUBLIC snap_core
  PRIVATE TBB::tbb
  OpenMP::OpenMP_CXX
  meshoptimizer
)