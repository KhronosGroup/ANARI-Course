## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if(TARGET Course::tinygltf)
  set(CourseTinyGltf_FOUND TRUE)
  return()
endif()

get_filename_component(_anari_course_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_library(course_tinygltf STATIC
  "${_anari_course_root}/third_party/tinygltf/tiny_gltf_v3.c"
  "${_anari_course_root}/third_party/tinygltf/tiny_gltf_v3.h"
  "${_anari_course_root}/third_party/tinygltf/tinygltf_json_c.h"
)
add_library(Course::tinygltf ALIAS course_tinygltf)

set_source_files_properties("${_anari_course_root}/third_party/tinygltf/tiny_gltf_v3.c"
  PROPERTIES
    LANGUAGE CXX
)

target_include_directories(course_tinygltf
  PUBLIC
    "${_anari_course_root}/third_party/tinygltf"
)

target_compile_features(course_tinygltf PUBLIC cxx_std_17)

set(CourseTinyGltf_FOUND TRUE)
