## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(ANARICourse07GltfLoader LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include(AnariCourseStep)
anari_course_configure()

find_package(anari REQUIRED)
find_package(thirteen MODULE REQUIRED)
find_package(CourseOrbit MODULE REQUIRED)
find_package(CourseTinyGltf MODULE REQUIRED)

anari_course_add_executable(07_gltf_loader
  main.cpp
  import_gltf.cpp
  import_gltf.h
)
target_link_libraries(07_gltf_loader
  PRIVATE
    thirteen::thirteen
    Course::orbit
    Course::tinygltf
)
