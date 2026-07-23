## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(ANARICourse02MultipleSurfaces LANGUAGES CXX)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../cmake")
include(AnariCourseStep)
anari_course_configure()

find_package(anari REQUIRED)
find_package(CourseStb MODULE REQUIRED)

anari_course_add_executable(02_multiple_surfaces main.cpp)
target_link_libraries(02_multiple_surfaces PRIVATE Course::stb)
