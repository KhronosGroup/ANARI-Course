## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(ANARICourse
  VERSION 0.1.0
  DESCRIPTION "SIGGRAPH ANARI course sample applications"
  LANGUAGES CXX
)

add_subdirectory(steps/01_triangle_png)
add_subdirectory(steps/02_multiple_surfaces)
add_subdirectory(steps/03_instanced_cubes)
add_subdirectory(steps/04_frame_size_and_camera)
add_subdirectory(steps/05_window_viewer)
add_subdirectory(steps/06_orbit_camera)
add_subdirectory(steps/07_gltf_loader)
add_subdirectory(steps/08_imgui_viewer)
