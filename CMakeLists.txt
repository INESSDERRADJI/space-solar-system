cmake_minimum_required(VERSION 3.10)
project(SolarSystem LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

# vcpkg packages (OK seulement si toolchain vcpkg est utilisé)
find_package(glfw3 CONFIG REQUIRED)
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
)

# forcer le .c en C (safe)
set_source_files_properties(libs/SOIL2/etc1_utils.c PROPERTIES LANGUAGE C)

target_include_directories(solarsystem PRIVATE
  ${CMAKE_SOURCE_DIR}/planet
  ${CMAKE_SOURCE_DIR}/libs
  ${CMAKE_SOURCE_DIR}/libs/SOIL2
)

target_link_libraries(solarsystem PRIVATE
  OpenGL::GL
  glfw
  glm::glm
  GLEW::GLEW
  assimp::assimp
)

# Copier resources/ à côté du .exe
add_custom_command(TARGET solarsystem POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/resources
          $<TARGET_FILE_DIR:solarsystem>/resources
)
