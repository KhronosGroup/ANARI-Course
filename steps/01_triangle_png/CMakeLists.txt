## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(ANARICourse01TrianglePNG LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include(AnariCourseStep)
anari_course_configure()

find_package(anari REQUIRED)
find_package(CourseStb MODULE REQUIRED)

anari_course_add_executable(01_triangle_png main.cpp)
target_link_libraries(01_triangle_png PRIVATE Course::stb)
