## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(ANARICourse08ImguiViewer LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include(AnariCourseStep)
anari_course_configure()

find_package(anari REQUIRED)
find_package(CourseOrbit MODULE REQUIRED)
find_package(CourseTinyGltf MODULE REQUIRED)
find_package(SDL3 MODULE REQUIRED)
find_package(imgui MODULE REQUIRED)

anari_course_add_executable(08_imgui_viewer
  main.cpp
  import_gltf.cpp
  import_gltf.h
)
target_link_libraries(08_imgui_viewer
  PRIVATE
    Course::orbit
    Course::tinygltf
    imgui
    SDL3::SDL3
)
