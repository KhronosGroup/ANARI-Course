## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if(TARGET Course::orbit)
  set(CourseOrbit_FOUND TRUE)
  return()
endif()

get_filename_component(_anari_course_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(course_orbit STATIC
  "${_anari_course_root}/third_party/orbit/Orbit.cpp"
  "${_anari_course_root}/third_party/orbit/Orbit.h"
)
add_library(Course::orbit ALIAS course_orbit)

target_link_libraries(course_orbit
  PUBLIC
    anari::anari
)

target_include_directories(course_orbit
  PUBLIC
    "${_anari_course_root}/third_party/orbit"
)

target_compile_features(course_orbit PUBLIC cxx_std_17)

set(CourseOrbit_FOUND TRUE)
