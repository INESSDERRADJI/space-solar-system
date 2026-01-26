# Space Solar System (OpenGL)

Interactive solar system scene built in C++ with OpenGL.  
The project uses CMake for building, GLFW/GLEW/GLM for rendering utilities, and GLSL shaders for lighting and post-processing.

## Features
- Real-time solar system rendering (Sun + planets)
- Planet orbits + optional orbit trajectories (toggle)
- Planet focus mode (keys 1–8) and free camera mode (0)
- Skybox environment
- Post-processing effects:
  - Bloom (toggle)
  - Lens flare (toggle)
  - Blur passes control

## Controls
- **W/A/S/D** or **Arrow keys**: move (only in free camera mode)  A ENLEVER 
- **Mouse wheel**: zoom (FOV)
- **0**: free camera
- **1..8**: focus a planet
- **P**: lock/unlock cursor
- **T**: toggle orbit trajectories
- **B**: toggle bloom
- **F**: toggle lens flare
- **ESC**: quit

## Tech Stack
- **Language:** C++
- **Graphics:** OpenGL + GLSL
- **Build:** CMake
- **Libraries:** GLFW, GLEW, GLM, stb_image
- **Dependency management:** vcpkg (manifest mode via `vcpkg.json`)

## Project Structure
space-solar-system/
├─ main.cpp
├─ CMakeLists.txt
├─ vcpkg.json
├─ planet/ # planet logic / rendering helpers
├─ resources/
│ ├─ shaders/ # GLSL shaders (.vs/.frag)
│ ├─ models/ # .obj models (planets, sun, etc.)
│ └─ skybox/ # cubemap textures
├─ ui/ # (optional / future UI work fusé tintin)
└─ build/ out/ .vs/ # generated (should be ignored)
