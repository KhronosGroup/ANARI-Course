// Copyright 2026 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include <anari/anari.h>

#define THIRTEEN_IMPLEMENTATION
#include <thirteen/thirteen.h>

#include "import_gltf.h"
#include "Orbit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t WIDTH = 1200;
constexpr uint32_t HEIGHT = 800;

struct Options
{
  const char *filename = "scene.gltf";
  const char *libraryName = "environment";
  const char *deviceName = "default";
};

void statusCallback(const void *,
    ANARIDevice,
    ANARIObject,
    ANARIDataType,
    ANARIStatusSeverity severity,
    ANARIStatusCode,
    const char *message)
{
  const char *label = "UNKNOWN";
  switch (severity) {
  case ANARI_SEVERITY_FATAL_ERROR:
    label = "FATAL";
    break;
  case ANARI_SEVERITY_ERROR:
    label = "ERROR";
    break;
  case ANARI_SEVERITY_WARNING:
    label = "WARN";
    break;
  case ANARI_SEVERITY_PERFORMANCE_WARNING:
    label = "PERF";
    break;
  case ANARI_SEVERITY_INFO:
    label = "INFO";
    break;
  case ANARI_SEVERITY_DEBUG:
    label = "DEBUG";
    break;
  default:
    break;
  }

  std::fprintf(stderr, "[ANARI:%s] %s\n", label, message ? message : "");
}

Options parseOptions(int argc, const char **argv)
{
  Options options;
  if (argc > 1)
    options.filename = argv[1];
  if (argc > 2)
    options.libraryName = argv[2];
  if (argc > 3)
    options.deviceName = argv[3];
  return options;
}

bool copyMappedFrameToWindow(
    ANARIDevice device, ANARIFrame frame, uint8_t *windowPixels)
{
  uint32_t width = 0;
  uint32_t height = 0;
  ANARIDataType pixelType = ANARI_UNKNOWN;
  const void *mappedFrame = anariMapFrame(
      device, frame, "channel.color", &width, &height, &pixelType);
  if (!mappedFrame || pixelType != ANARI_UFIXED8_RGBA_SRGB) {
    std::fprintf(stderr, "ANARI frame did not return RGBA8 color data\n");
    if (mappedFrame)
      anariUnmapFrame(device, frame, "channel.color");
    return false;
  }

  const auto *srcPixels = static_cast<const uint8_t *>(mappedFrame);
  const size_t rowBytes = size_t(width) * 4;
  for (uint32_t y = 0; y < height; ++y) {
    const uint32_t srcY = height - 1 - y;
    std::memcpy(windowPixels + size_t(y) * rowBytes,
        srcPixels + size_t(srcY) * rowBytes,
        rowBytes);
  }

  anariUnmapFrame(device, frame, "channel.color");
  return true;
}

bool updateCamera(ANARIDevice device,
    ANARICamera camera,
    const anari_viewer::manipulators::Orbit &orbit,
    uint32_t width,
    uint32_t height)
{
  const anari::math::float3 position = orbit.eye();
  const anari::math::float3 direction = orbit.dir();
  const anari::math::float3 up = orbit.up();
  const float aspect = float(width) / float(height);

  anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
  anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, &position);
  anariSetParameter(
      device, camera, "direction", ANARI_FLOAT32_VEC3, &direction);
  anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, &up);
  anariCommitParameters(device, camera);
  return true;
}

void resetCameraFromWorldBounds(ANARIDevice device,
    ANARIWorld world,
    ANARICamera camera,
    anari_viewer::manipulators::Orbit &orbit)
{
  anari::math::float3 bounds[2] = {
      anari::math::float3(-1.f, 0.f, -1.f),
      anari::math::float3(1.f, 2.f, 1.f),
  };

  if (!anariGetProperty(device,
          world,
          "bounds",
          ANARI_FLOAT32_BOX3,
          bounds,
          sizeof(bounds),
          ANARI_WAIT)) {
    std::fprintf(stderr, "WARNING: bounds not returned; using fallback\n");
  }

  const anari::math::float3 center = 0.5f * (bounds[0] + bounds[1]);
  const anari::math::float3 diagonal = bounds[1] - bounds[0];
  const float distance = std::max(1.25f * linalg::length(diagonal), 1.f);
  orbit.setConfig(center, distance, anari::math::float2(0.f, 0.f));
  updateCamera(device, camera, orbit, WIDTH, HEIGHT);
}

void handleCameraInput(anari_viewer::manipulators::Orbit &orbit)
{
  static bool mouseRotating = false;
  const bool dolly = Thirteen::GetMouseButton(1);
  const bool pan = Thirteen::GetMouseButton(2);
  const bool rotate = Thirteen::GetMouseButton(0);
  const bool anyMovement = dolly || pan || rotate;

  if (!anyMovement) {
    mouseRotating = false;
    return;
  }

  int mouseX = 0;
  int mouseY = 0;
  int prevMouseX = 0;
  int prevMouseY = 0;
  Thirteen::GetMousePosition(mouseX, mouseY);
  Thirteen::GetMousePositionLastFrame(prevMouseX, prevMouseY);

  if (!Thirteen::GetMouseButtonLastFrame(0)
      && !Thirteen::GetMouseButtonLastFrame(1)
      && !Thirteen::GetMouseButtonLastFrame(2))
    return;

  const anari::math::float2 mouseFrom{float(prevMouseX), float(prevMouseY)};
  const anari::math::float2 mouseTo{float(mouseX), float(mouseY)};
  const anari::math::float2 viewportSize{float(WIDTH), float(HEIGHT)};
  const anari::math::float2 mouseDelta =
      (mouseTo * 2.f / viewportSize) - (mouseFrom * 2.f / viewportSize);

  if (mouseDelta == anari::math::float2(0.f))
    return;

  if (dolly) {
    orbit.zoom(mouseDelta.y);
    mouseRotating = false;
  } else if (pan) {
    orbit.pan(mouseDelta);
    mouseRotating = false;
  } else if (rotate) {
    if (!mouseRotating) {
      orbit.startNewRotation();
      mouseRotating = true;
    }
    orbit.rotate(anari::math::float2(-mouseDelta.x, -mouseDelta.y));
  }
}

} // namespace

int main(int argc, const char **argv)
{
  const Options options = parseOptions(argc, argv);

  Thirteen::SetApplicationName("ANARI Course 07 - glTF Loader");
  uint8_t *windowPixels = Thirteen::Init(WIDTH, HEIGHT, false);
  if (!windowPixels) {
    std::fprintf(stderr, "Could not initialize Thirteen\n");
    return 1;
  }

  ANARILibrary library =
      anariLoadLibrary(options.libraryName, statusCallback, nullptr);
  if (!library) {
    std::fprintf(
        stderr, "Failed to load ANARI library '%s'\n", options.libraryName);
    Thirteen::Shutdown();
    return 1;
  }

  ANARIDevice device = anariNewDevice(library, options.deviceName);
  if (!device) {
    std::fprintf(
        stderr, "Failed to create ANARI device '%s'\n", options.deviceName);
    Thirteen::Shutdown();
    return 1;
  }

  ANARIWorld world = import_gltf(device, options.filename);
  if (!world) {
    anariRelease(device, device);
    Thirteen::Shutdown();
    return 1;
  }

  // Create a camera; the Orbit helper will fill in its view parameters.
  ANARICamera camera = anariNewCamera(device, "perspective");
  anari_viewer::manipulators::Orbit orbit;
  resetCameraFromWorldBounds(device, world, camera, orbit);

  // Configure the renderer and use ambient light for the matte material.
  ANARIRenderer renderer = anariNewRenderer(device, "default");
  const float background[4] = {0.02f, 0.025f, 0.03f, 1.f};
  const float ambientRadiance = 1.f;
  anariSetParameter(
      device, renderer, "background", ANARI_FLOAT32_VEC4, &background[0]);
  anariSetParameter(
      device, renderer, "ambientRadiance", ANARI_FLOAT32, &ambientRadiance);
  anariCommitParameters(device, renderer);

  // Create the frame that connects renderer, camera, world, and color output.
  ANARIFrame frame = anariNewFrame(device);
  const uint32_t frameSize[2] = {WIDTH, HEIGHT};
  ANARIDataType colorFormat = ANARI_UFIXED8_RGBA_SRGB;
  anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, &frameSize[0]);
  anariSetParameter(
      device, frame, "channel.color", ANARI_DATA_TYPE, &colorFormat);
  anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
  anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
  anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
  anariCommitParameters(device, frame);

  // Render continuously, map the color channel, and copy pixels to the window.
  bool ok = true;
  while (ok && Thirteen::Render() && !Thirteen::GetKey(VK_ESCAPE)) {
    handleCameraInput(orbit);
    updateCamera(device, camera, orbit, WIDTH, HEIGHT);
    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);
    ok = copyMappedFrameToWindow(device, frame, windowPixels);
  }

  anariRelease(device, frame);
  anariRelease(device, renderer);
  anariRelease(device, camera);
  anariRelease(device, world);
  anariRelease(device, device);
  anariUnloadLibrary(library);
  Thirteen::Shutdown();

  if (!ok) {
    return 1;
  }

  return 0;
}
