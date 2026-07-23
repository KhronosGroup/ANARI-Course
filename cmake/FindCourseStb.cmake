## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if(TARGET Course::stb)
  set(CourseStb_FOUND TRUE)
  return()
endif()

get_filename_component(_anari_course_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(course_stb INTERFACE)
add_library(Course::stb ALIAS course_stb)

target_include_directories(course_stb
  INTERFACE
    "${_anari_course_root}/third_party/stb"
)

set(CourseStb_FOUND TRUE)
