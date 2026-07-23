## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

function(anari_course_prefer_repo_archive output_variable repo_archive fallback_url)
  if(EXISTS "${repo_archive}")
    set(${output_variable} "${repo_archive}" PARENT_SCOPE)
  else()
    set(${output_variable} "${fallback_url}" PARENT_SCOPE)
  endif()
endfunction()

function(anari_course_copy_local_archive_to_repo package archive_name repo_archive)
  get_filename_component(_anari_course_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  if(CMAKE_SOURCE_DIR STREQUAL _anari_course_root OR EXISTS "${repo_archive}")
    return()
  endif()

  set(_local_archive "${CMAKE_SOURCE_DIR}/.anari_deps/${package}/${archive_name}")
  if(EXISTS "${_local_archive}")
    get_filename_component(_repo_archive_dir "${repo_archive}" DIRECTORY)
    file(MAKE_DIRECTORY "${_repo_archive_dir}")
    file(COPY_FILE "${_local_archive}" "${repo_archive}" ONLY_IF_DIFFERENT)
  endif()
endfunction()

function(anari_course_remove_local_dependency_cache package)
  get_filename_component(_anari_course_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  if(CMAKE_SOURCE_DIR STREQUAL _anari_course_root)
    return()
  endif()

  set(_local_deps_dir "${CMAKE_SOURCE_DIR}/.anari_deps")
  file(REMOVE_RECURSE "${_local_deps_dir}/${package}")

  if(EXISTS "${_local_deps_dir}")
    file(GLOB _local_dep_entries LIST_DIRECTORIES true "${_local_deps_dir}/*")
    if(NOT _local_dep_entries)
      file(REMOVE_RECURSE "${_local_deps_dir}")
    endif()
  endif()
endfunction()
