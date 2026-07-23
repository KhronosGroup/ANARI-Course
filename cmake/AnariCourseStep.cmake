## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

function(anari_course_configure)
  set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
  set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
  set(CMAKE_CXX_EXTENSIONS OFF PARENT_SCOPE)

  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" PARENT_SCOPE)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" PARENT_SCOPE)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" PARENT_SCOPE)
endfunction()

function(anari_course_add_executable target)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE anari::anari)
  target_compile_features(${target} PRIVATE cxx_std_17)
endfunction()
