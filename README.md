<!--
Copyright 2026 The Khronos Group
SPDX-License-Identifier: Apache-2.0
-->

# ANARI Course Example Code

This repository contains the example applications for the SIGGRAPH course
introducing the ANARI C API. The examples are designed for recorded and
self-paced students: each checkpoint is a complete application that can be read
and run independently.

The course starts with offline PNG rendering, then moves to a fixed-size window,
orbit camera interaction, glTF loading, and finally an ImGui-based viewer that
students can extend.

## Build

```sh
cmake -S . -B build
cmake --build build
```

The build requires an [ANARI-SDK](https://github.com/KhronosGroup/ANARI-SDK)
installation. The final ImGui checkpoint fetches SDL 3 and Dear ImGui during
CMake configure through `anari_sdk_fetch_project()`.
On Linux, the windowed checkpoints still require platform windowing and graphics
development headers for Thirteen and SDL's renderer backends.

## Runtime Selection

The examples default to:

```text
library: environment
device:  default
```

Most checkpoints allow optional runtime overrides. For example:

```sh
./build/anari_course_01_triangle_png triangle.png visrtx default
./build/anari_course_05_window_viewer visrtx default
```

## Course Steps

### 1. Render Your First Triangle

Folder: `steps/01_triangle_png`

Executable:

```sh
./build/anari_course_01_triangle_png triangle.png
```

Concepts:

- Load an ANARI library and create a device.
- Create triangle geometry with mapped parameter arrays.
- Create a required `matte` material.
- Connect geometry and material through a surface.
- Attach the surface to a world.
- Configure camera, renderer, frame, and `channel.color`.
- Render once, map the frame, and write an RGBA PNG.

### 2. Build A Small Scene

Folder: `steps/02_multiple_surfaces`

Executable:

```sh
./build/anari_course_02_multiple_surfaces multiple_surfaces.png
```

Concepts:

- Build a box-on-floor scene.
- Use multiple surfaces in `world.surface`.
- Use separate matte materials for each surface.
- Use per-vertex color for the box and constant material color for the floor.
- Keep rendering offline to PNG.

### 3. Instance The Cube

Folder: `steps/03_instanced_cubes`

Executable:

```sh
./build/anari_course_03_instanced_cubes instanced_cubes.png
```

Concepts:

- Keep the floor and cube data from step 2.
- Put the cube surface in an ANARI group.
- Create several `transform` instances that share that group.
- Set per-instance transform matrices to place cubes over the floor.
- Attach the floor through `world.surface` and cubes through `world.instance`.
- Keep rendering offline to PNG.

### 4. Control The Image And Camera

Folder: `steps/04_frame_size_and_camera`

Executable:

```sh
./build/anari_course_04_frame_size_and_camera frame_size_and_camera.png 1920 1080
```

Concepts:

- Keep the same box/floor scene while making camera and image size explicit.
- Make output resolution a frame parameter.
- Update camera aspect from the requested image size.
- Keep the view hard-coded before introducing interaction.

Command-line form:

```text
anari_course_04_frame_size_and_camera [output.png] [width height] [library device]
```

### 5. Put ANARI Pixels In A Window

Folder: `steps/05_window_viewer`

Executable:

```sh
./build/anari_course_05_window_viewer
```

Concepts:

- Keep the same scene and camera from step 4.
- Render continuously with blocking `ANARI_WAIT`.
- Map `channel.color` each frame.
- Copy mapped pixels into a fixed-size 1200x800 Thirteen window.
- Treat the window as another destination for ANARI pixels.

### 6. Connect Interaction To The Camera

Folder: `steps/06_orbit_camera`

Executable:

```sh
./build/anari_course_06_orbit_camera
```

Concepts:

- Use the vendored ANARI-SDK `Orbit` helper as app-side camera infrastructure.
- Query `world.bounds` to initialize the orbit camera.
- Convert orbit output into ANARI camera `position`, `direction`, and `up`.
- Commit the camera after mouse interaction changes its parameters.
- Continue using the simple blocking render loop.

Controls:

```text
LMB: orbit
MMB: pan
RMB: dolly
Esc: exit
```

### 7. Load A glTF Scene

Folder: `steps/07_gltf_loader`

Executable:

```sh
./build/anari_course_07_gltf_loader scene.gltf
```

Concepts:

- Keep the orbit-camera viewer shell from step 6.
- Move scene construction into a separate `import_gltf()` function.
- Introduce vendored tinygltf v3 as the parser used by the loader.
- Return an `ANARIWorld` from the loader so the viewer remains scene-agnostic.

Command-line form:

```text
anari_course_07_gltf_loader [scene.gltf] [library device]
```

### 8. Turn It Into An Extensible Viewer

Folder: `steps/08_imgui_viewer`

Executable:

```sh
./build/anari_course_08_imgui_viewer scene.gltf
```

Concepts:

- Move to an SDL 3 renderer/ImGui application shell.
- Load the same glTF scene input used by step 7.
- Support framebuffer resizing by updating the ANARI frame size and camera aspect.
- Render continuously using a simple `frameInFlight` polling model.
- Keep the UI responsive by checking `anariFrameReady(..., ANARI_NO_WAIT)`.
- Expose renderer parameters through ImGui controls.

Command-line form:

```text
anari_course_08_imgui_viewer [scene.gltf] [library device]
```

Controls:

```text
LMB: orbit
MMB: pan
RMB: dolly
Esc: exit
```

## Supporting Material

- `docs/course-design-summary.md` summarizes the course design decisions.
- `slides/outline.md` gives the 90-minute presentation structure.
- `slides/*.md` contains first-draft slide content for each section.

## Third-Party Code

- `third_party/thirteen` is a small window/pixel display helper.
- `third_party/orbit` contains the ANARI-SDK orbit camera helper.
- `third_party/stb/stb_image_write.h` is vendored for PNG output.
- `third_party/tinygltf` contains the vendored tinygltf v3 parser.
- SDL 3 and Dear ImGui are fetched by CMake for the final checkpoint.
