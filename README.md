# 🌿 Interactive Plant Modeler

> Real-time 3D plant modeling application — CPR 557 Final Project, Iowa State University, Spring 2026

![C++](https://img.shields.io/badge/C++-17-00599C?style=flat&logo=cplusplus&logoColor=white)
![Vulkan](https://img.shields.io/badge/Vulkan-API-AC162C?style=flat&logo=vulkan&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey?style=flat)
![Phases](https://img.shields.io/badge/Phases-7%2F7%20Complete-brightgreen?style=flat)

---

## Overview

**Interactive Plant Modeler** is a Vulkan-based graphics system built in C++17 that procedurally generates a potted plant scene using Bézier geometry. It supports real-time pot design at runtime and simulates wind-driven deformation through GPU vertex processing.

<div align="center">
  <img src="assets/demo.gif" alt="demo" width="700">
</div>

---

## Features

### Core
- **Procedural Bézier geometry** — stem, leaves, and pot built from Bézier profiles evaluated with the de Casteljau algorithm
- **Surface of revolution** — 2D Bézier profile revolved around the Y-axis to produce 3D geometry with analytic normals
- **Hierarchical scene graph** — `plant_root → pot, stem, leaves_group → leaf1/2/3` with parent-to-child transform propagation
- **Interactive pot editor** — click to place control points, press `B` to generate a new pot live at runtime
- **Camera navigation** — orbit, pan, zoom, twist, and fit-all
- **Wind simulation** — GPU vertex deformation using a cantilever beam model; direction set via mouse drag

### Stretch Goal — Texture Mapping

| Object | Mode | Description |
|--------|------|-------------|
| Pot    | Image texture | `pot_texture.png` loaded with `stb_image`, uploaded as `VkImage`, sampled in GLSL |
| Leaves | Procedural shader | Axial gradient · tapered midrib · 7 curved lateral veins · 8 sub-veins · micro-surface noise |
| Stem   | Procedural shader | Base-to-tip gradient · 8 vertical stripes · horizontal node bands · highlight strip |

---

## Build & Run

**Requirements:** Vulkan SDK, GLFW 3.4, GLM — installed and on `PATH` / `VULKAN_SDK`

```bash
# Step 1: Compile shaders (generates .spv files)
# Windows:
compile-win.bat
# macOS / Linux:
bash compile-unx.bat

# Step 2: Build
make

# Step 3: Run
./FinalProject        # macOS/Linux
FinalProject.exe      # Windows
```

---

## Controls

### Mode
| Key | Action |
|-----|--------|
| `Space` | Toggle Edit Mode / Navigation Mode |
| `C` | Toggle perspective / orthographic projection |

### Navigation Mode
| Input | Action |
|-------|--------|
| `R` + drag | Orbit (rotate) camera |
| `P` + drag | Pan camera |
| `Z` + drag | Zoom |
| `T` + drag | Twist (roll) camera |
| `F` | Fit-all — frame the full scene |
| Arrow keys | Rotate camera |
| `Shift` + arrows | Pan camera |
| `=` / `-` | Zoom in / out |

### Scene Graph
| Key | Action |
|-----|--------|
| `N` | Select next node (DFS order); selected node spins |
| `Tab` | Print scene graph hierarchy to terminal |
| `Shift+N` | Toggle surface normal vectors |

### Edit Mode
| Key | Action |
|-----|--------|
| Left click | Place a Bézier control point (upper half of screen) |
| `B` | Build surface of revolution from current points |
| `M` | Clear all control points |
| `X` | Toggle helical twist |
| `G` | Toggle rainbow color gradient |

### Wind
| Input | Action |
|-------|--------|
| `W` + drag | Set wind direction — leaves deform toward cursor |

---

## Tech Stack

| Technology | Role |
|------------|------|
| Vulkan | Rendering pipeline (swap chain, descriptors, push constants, depth buffer) |
| GLSL / SPIR-V | Vertex + fragment shaders compiled offline with `glslc` |
| GLFW | Window creation and input |
| GLM | Math (vectors, matrices, transforms) |
| stb_image | PNG/JPG texture loading (single-header) |
| C++17 | Application logic |
| MoltenVK | Vulkan-to-Metal translation on macOS Apple Silicon |

---

## Project Structure

```
.
├── main.cpp
├── my_application.cpp / .h             # Scene setup, texture loading, wind, animation
├── my_bezier_curve_surface.cpp / .h    # Bézier evaluation, surface of revolution
├── my_camera.cpp / .h                  # Orbit / pan / zoom / twist / fit-all
├── my_game_object.cpp / .h             # Scene graph node + transform hierarchy
├── my_keyboard_controller.cpp / .h     # Input dispatch
├── my_simple_render_factory.cpp / .h   # DFS render traversal
├── my_model.cpp / .h                   # Vertex / index buffer
├── my_buffer.cpp / .h                  # Vulkan buffer helper
├── my_device.cpp / .h                  # Vulkan device + command helpers
├── my_renderer.cpp / .h                # Swap chain render loop
├── my_pipeline.cpp / .h                # Graphics pipeline
├── my_descriptors.cpp / .h             # Descriptor pool / set / layout
├── my_swap_chain.cpp / .h              # Swap chain management
└── assets/
  ├── demo.gif                       
  ├── shaders/
  │   ├── simple_shader.vert / .frag    # Phong lighting + texture selection
  │   └── point_curve_shader.vert/.frag # Edit-mode control point overlay
  └── textures/
      └── pot_texture.png
```

---

## Known Limitations

- Wind deformation is a linear cantilever approximation; extreme directions may cause visible stretching
- The plant model is pre-built and hardcoded; users cannot import or customize plant geometry at runtime
- Only one plant can be displayed in the scene at a time
- Bézier control points cannot be adjusted after placement; must clear all and restart with `M`

---

## Potential Future Improvements

- Physics-based spring–damper wind model replacing the linear cantilever approximation
- Adding a GUI bar to allow user to import .obj files and texture image files, or to export generated pot geometry to OBJ/glTF for use in external tools
- GUI overlay (Dear ImGui) for real-time control-point editing with drag handles
- Multi-plant scene with instanced rendering

---
## Source

https://github.com/kibonku/CPR557_Final_Project
