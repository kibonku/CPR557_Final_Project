# Interactive Plant Modeler

A real-time 3D plant modeling application built with **Vulkan** for CPRE 557 (Computer Graphics). Procedurally generates plant geometry from Bézier curves, supports interactive pot design via surface of revolution, and simulates wind response through a hierarchical scene graph.

---

## Features

### Core
- **Procedural Bézier geometry** — all surfaces (pot, stem, leaves) are built from Bézier profiles evaluated with the de Casteljau algorithm
- **Surface of revolution** — a 2D Bézier profile is revolved around an axis to produce 3D geometry with analytic normals
- **Hierarchical scene graph** — `plant_root → pot, stem, leaves_group → leaf1, leaf2, leaf3` with parent-to-child transform propagation
- **Interactive pot editor** — place control points by clicking, then press `B` to generate a new pot live at runtime
- **Camera navigation** — orbit, pan, zoom, twist, and fit-all
- **Wind simulation** — GPU vertex deformation using a cantilever beam model; wind direction set by mouse

### Stretch Goal — Texture Mapping
Three rendering modes, selected per-object via push constants:

| Object | Mode | Description |
|--------|------|-------------|
| Pot | Image texture | `pot_texture.png` loaded with stb_image, uploaded as `VkImage`, sampled in GLSL |
| Leaves | Procedural shader | Axial gradient · tapered midrib · 7 curved lateral veins · 8 sub-veins · micro-surface noise |
| Stem | Procedural shader | Base-to-tip gradient · 8 vertical stripes · horizontal node bands · highlight strip |

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

### Wind
| Input | Action |
|-------|--------|
| `W` + mouse move | Set wind direction — leaves deform toward cursor |

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

---

## Build

**Requirements:** Vulkan SDK, GLFW 3.4, GLM — installed and on `PATH` / `VULKAN_SDK`.

```bash
# Compile shaders first (generates .spv files)
# Windows:
compile-win.bat

# macOS / Linux:
bash compile-unx.bat

# Build application
make

# Run
./FinalProject        # macOS/Linux
FinalProject.exe      # Windows
```

---

## Tech Stack

| Technology | Role |
|------------|------|
| Vulkan | Rendering pipeline (swap chain, descriptors, push constants, depth buffer) |
| GLSL (SPIR-V) | Vertex + fragment shaders compiled offline with `glslc` |
| GLFW | Window creation and input |
| GLM | Math (vectors, matrices, transforms) |
| stb_image | PNG/JPG texture loading (single-header) |
| C++17 | Application logic |

---

## Project Structure

```
.
├── main.cpp                          # Entry point
├── my_application.cpp / .h           # Scene setup, texture loading, wind, animation
├── my_bezier_curve_surface.cpp / .h  # Bézier evaluation, surface of revolution
├── my_camera.cpp / .h                # Orbit / pan / zoom / twist / fit-all
├── my_game_object.cpp / .h           # Scene graph node + transform hierarchy
├── my_keyboard_controller.cpp / .h   # Input dispatch
├── my_simple_render_factory.cpp / .h # DFS render traversal
├── my_model.cpp / .h                 # Vertex / index buffer
├── my_buffer.cpp / .h                # Vulkan buffer helper
├── my_device.cpp / .h                # Vulkan device + command helpers
├── my_renderer.cpp / .h              # Swap chain render loop
├── my_pipeline.cpp / .h              # Graphics pipeline
├── my_descriptors.cpp / .h           # Descriptor pool / set / layout
├── my_swap_chain.cpp / .h            # Swap chain management
├── shaders/
│   ├── simple_shader.vert / .frag    # Phong lighting + texture selection
│   └── point_curve_shader.vert/.frag # Edit-mode control point overlay
└── textures/
    └── pot_texture.png               # Pot surface texture
```

---

## Course Context

CPRE 557 — Computer Graphics, Iowa State University  
Final project implementing: Bézier surfaces · surface of revolution · analytic normals · UV mapping · hierarchical scene graph · DFS traversal · Phong shading · Vulkan pipeline
