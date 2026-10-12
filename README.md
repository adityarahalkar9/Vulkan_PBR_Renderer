# Vulkan PBR Renderer

A real-time physically-based rendering engine built with C++20, Vulkan 1.3+, and SDL3.

## Features

- Physically-Based Rendering with metallic-roughness workflow
- Cook-Torrance specular BRDF (GGX distribution, Smith geometry, Schlick Fresnel)
- Directional light shadow mapping with PCF filtering
- Up to 4 dynamic lights (directional and point)
- GPU memory management via Vulkan Memory Allocator
- Per-swapchain-image command buffer allocation
- Validation layer integration with severity-based logging

## Prerequisites

- CMake 3.25 or higher
- Vulkan SDK 1.3 or higher (provides `glslangValidator`)
- Visual Studio 2022 or higher with C++ workload
- vcpkg (Microsoft C++ package manager)

## Building

### Clone

```bash
git clone https://github.com/adityarahalkar9/Vulkan_PBR_Renderer.git
cd Vulkan_PBR_Renderer
```

### Install Dependencies

The project uses vcpkg manifest mode. All dependencies are declared in `vcpkg.json` and install automatically when CMake configures with the vcpkg toolchain.

```bash
vcpkg install
```

### Configure and Build

Find your vcpkg installation path:

```powershell
(Get-Command vcpkg).Source
```

Then configure and build (replace `[vcpkg-root]` with your actual path):

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="[vcpkg-root]/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release
```

### Run

```bash
cd build/bin
VulkanPBR.exe
```

> **Note:** The executable must be run from `build/bin` because it loads compiled shaders relative to the current working directory.

## Controls

| Key | Action |
|---|---|
| W / A / S / D | Move camera |
| Space | Move up |
| Shift | Move down |
| Left Mouse + Drag | Look around |
| Close Window | Exit |

## Project Structure

```text
Vulkan_PBR_Renderer/
├── CMakeLists.txt
├── vcpkg.json
├── README.md
│
├── shaders/
│   ├── pbr.vert
│   ├── pbr.frag
│   ├── shadow.vert
│   ├── shadow.frag
│   └── compile_shaders.bat
│
└── src/
    ├── main.cpp
    │
    ├── common/
    │   └── types.h
    │
    ├── core/
    │   ├── timer.h
    │   └── timer.cpp
    │
    ├── memory/
    │   ├── buffer.h
    │   ├── buffer.cpp
    │   ├── image.h
    │   └── image.cpp
    │
    ├── renderer/
    │   ├── vulkancontext.h
    │   ├── vulkancontext.cpp
    │   ├── swapchain.h
    │   ├── swapchain.cpp
    │   ├── renderer.h
    │   ├── renderer.cpp
    │   ├── mesh_loader.h
    │   ├── mesh_loader.cpp
    │   ├── texture_loader.h
    │   └── texture_loader.cpp
    │
    ├── scene/
    │   ├── scene.h
    │   ├── scene.cpp
    │   └── transform.h
    │
    └── utils/
        ├── logger.h
        └── vulkanutils.h
```

## Rendering Pipeline

Each frame performs the following steps:

1. Acquire swapchain image with semaphore synchronization.
2. Update camera and light uniform buffers.
3. Shadow pass: Render scene depth from directional light perspective into a $2048 \times 2048$ depth attachment.
4. Main pass: Render scene with PBR shading and shadow map sampling.
5. Submit to graphics queue with fence synchronization.
6. Present to swapchain.

## Technical Details

The renderer uses three descriptor set layouts:

- **Set 0 (Global):** Contains camera and lighting uniform buffers.
- **Set 1 (Material):** Contains five PBR texture samplers plus a material parameters uniform buffer.
- **Set 2 (Shadow):** Contains the shadow map sampler. 

Model matrices are passed via push constants. Shadows use a $3 \times 3$ PCF kernel with $N \cdot L$-scaled slope bias to minimize shadow acne.

## Dependencies

| Library | License | Purpose |
|---|---|---|
| SDL3 | Zlib | Windowing and input |
| GLM | MIT | Math operations |
| Vulkan Memory Allocator | MIT | GPU memory allocation |
| stb_image | Public Domain | Texture loading |
| Dear ImGui | MIT | Reserved for debug UI |

## License

GNU General Public License v3.0. See `LICENSE.txt` for details.