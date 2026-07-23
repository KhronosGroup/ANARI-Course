## Copyright 2026 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

add_library(thirteen INTERFACE)
add_library(thirteen::thirteen ALIAS thirteen)

target_include_directories(thirteen
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_compile_features(thirteen INTERFACE cxx_std_17)

if(WIN32)
  target_link_libraries(thirteen
    INTERFACE
      d3d12
      dxgi
      user32
      xinput
  )
elseif(APPLE)
  target_compile_definitions(thirteen INTERFACE TARGET_OS_OSX=1)
  target_link_libraries(thirteen
    INTERFACE
      "-framework Cocoa"
      "-framework CoreGraphics"
      "-framework Metal"
      "-framework QuartzCore"
  )
elseif(UNIX)
  find_package(OpenGL REQUIRED)
  find_package(X11 REQUIRED)

  target_include_directories(thirteen
    INTERFACE
      ${X11_INCLUDE_DIR}
      ${OPENGL_INCLUDE_DIR}
  )

  target_link_libraries(thirteen
    INTERFACE
      ${X11_LIBRARIES}
      OpenGL::GL
      ${CMAKE_DL_LIBS}
  )
endif()
