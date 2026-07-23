## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(ANARICourse04FrameSizeAndCamera LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include(AnariCourseStep)
anari_course_configure()

find_package(anari REQUIRED)
find_package(CourseStb MODULE REQUIRED)

anari_course_add_executable(04_frame_size_and_camera main.cpp)
target_link_libraries(04_frame_size_and_camera PRIVATE Course::stb)
