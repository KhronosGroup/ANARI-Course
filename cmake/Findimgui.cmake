## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if(TARGET imgui)
  set(imgui_FOUND TRUE)
  return()
endif()

if(NOT COMMAND anari_sdk_fetch_project)
  message(FATAL_ERROR "find_package(anari REQUIRED) must run before find_package(imgui REQUIRED)")
endif()

get_filename_component(_anari_course_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include(AnariCourseFetch)

find_package(SDL3 MODULE REQUIRED)

set(_imgui_url "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.7-docking.zip")
set(_imgui_archive "${_anari_course_root}/.anari_deps/imgui/v1.91.7-docking.zip")
anari_course_prefer_repo_archive(_imgui_url "${_imgui_archive}" "${_imgui_url}")

anari_sdk_fetch_project(
  NAME imgui
  URL "${_imgui_url}"
  MD5 2cfbf7b7790076d6debe5060ec1fb47f
)
anari_course_copy_local_archive_to_repo(imgui v1.91.7-docking.zip "${_imgui_archive}")
anari_course_remove_local_dependency_cache(imgui)

add_library(imgui STATIC
  "${imgui_LOCATION}/imgui.cpp"
  "${imgui_LOCATION}/imgui_draw.cpp"
  "${imgui_LOCATION}/imgui_tables.cpp"
  "${imgui_LOCATION}/imgui_widgets.cpp"
  "${imgui_LOCATION}/backends/imgui_impl_sdl3.cpp"
  "${imgui_LOCATION}/backends/imgui_impl_sdlrenderer3.cpp"
)
add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui
  PUBLIC
    "${imgui_LOCATION}"
    "${imgui_LOCATION}/backends"
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
target_compile_features(imgui PUBLIC cxx_std_17)

set(imgui_FOUND TRUE)
