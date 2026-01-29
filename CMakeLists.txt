cmake_minimum_required(VERSION 3.10)
project(SolarSystem LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenGL
find_package(OpenGL REQUIRED)


# vcpkg packages 
find_package(glfw3 CONFIG REQUIRED)
>>>>>>> UI-fusée
find_package(glm CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

add_executable(solarsystem
  main.cpp

  planet/planet.cpp
  planet/planet.hpp
  planet/camera.h
  planet/shader.h
  planet/model.h
  planet/mesh.h

  libs/stb_image.cpp
  libs/SOIL2/etc1_utils.c

  # ImGui 
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/backends/imgui_impl_glfw.cpp
  imgui/backends/imgui_impl_opengl3.cpp
)

# Forcer le .c en C
set_source_files_properties(libs/SOIL2/etc1_utils.c PROPERTIES LANGUAGE C)

target_include_directories(solarsystem PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/planet
  ${CMAKE_SOURCE_DIR}/libs
  ${CMAKE_SOURCE_DIR}/libs/SOIL2
  ${CMAKE_SOURCE_DIR}/imgui
  ${CMAKE_SOURCE_DIR}/imgui/backends
)

target_link_libraries(solarsystem PRIVATE
  OpenGL::GL
  glfw
  glm::glm
  GLEW::GLEW
  assimp::assimp
)

# Copier resources à côté du .exe
add_custom_command(TARGET solarsystem POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/resources
          $<TARGET_FILE_DIR:solarsystem>/resources
)
