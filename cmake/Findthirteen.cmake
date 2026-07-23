## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if(TARGET thirteen::thirteen)
  set(thirteen_FOUND TRUE)
  return()
endif()

get_filename_component(_anari_course_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

add_subdirectory(
  "${_anari_course_root}/third_party/thirteen"
  "${CMAKE_BINARY_DIR}/third_party/thirteen"
)

set(thirteen_FOUND TRUE)
