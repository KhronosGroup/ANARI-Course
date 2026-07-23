## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if(TARGET SDL3::SDL3)
  set(SDL3_FOUND TRUE)
  return()
endif()

if(NOT COMMAND anari_sdk_fetch_project)
  message(FATAL_ERROR "find_package(anari REQUIRED) must run before find_package(SDL3 REQUIRED)")
endif()

get_filename_component(_anari_course_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include(AnariCourseFetch)

set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

set(_sdl_url "https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.2.0.zip")
set(_sdl_archive "${_anari_course_root}/.anari_deps/SDL/release-3.2.0.zip")
anari_course_prefer_repo_archive(_sdl_url "${_sdl_archive}" "${_sdl_url}")

anari_sdk_fetch_project(
  NAME SDL
  URL "${_sdl_url}"
  MD5 c8b5efd263936b157e63bf5c1fad4bc6
  ADD_SUBDIR
)
anari_course_copy_local_archive_to_repo(SDL release-3.2.0.zip "${_sdl_archive}")
anari_course_remove_local_dependency_cache(SDL)

if(NOT TARGET SDL3::SDL3)
  message(FATAL_ERROR "SDL fetch completed but did not create SDL3::SDL3")
endif()

set(SDL3_FOUND TRUE)
