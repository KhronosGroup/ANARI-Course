// Copyright 2026 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include <anari/anari.h>
#include <anari/frontend/type_utility.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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
  const char *outputFile = "multiple_surfaces.png";
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

void setParameterArray1D(ANARIDevice device,
    ANARIObject object,
    const char *name,
    ANARIDataType type,
    const void *values,
    uint64_t count)
{
  const size_t srcStride = anari::sizeOf(type);
  if (srcStride == 0) {
    std::fprintf(stderr, "Unsupported ANARI array element type\n");
    return;
  }

  uint64_t dstStride = 0;
  auto *dst = static_cast<uint8_t *>(
      anariMapParameterArray1D(device, object, name, type, count, &dstStride));
  if (!dst) {
    std::fprintf(stderr, "Failed to map parameter array '%s'\n", name);
    return;
  }

  if (dstStride == 0)
    dstStride = srcStride;

  const auto *src = static_cast<const uint8_t *>(values);
  for (uint64_t i = 0; i < count; ++i)
    std::memcpy(dst + i * dstStride, src + i * srcStride, srcStride);

  anariUnmapParameterArray(device, object, name);
}

Options parseOptions(int argc, const char **argv)
{
  Options options;
  if (argc > 1)
    options.outputFile = argv[1];
  if (argc > 2)
    options.libraryName = argv[2];
  if (argc > 3)
    options.deviceName = argv[3];
  return options;
}

void normalizeDirection(const float from[3], const float to[3], float dir[3])
{
  dir[0] = to[0] - from[0];
  dir[1] = to[1] - from[1];
  dir[2] = to[2] - from[2];
  const float length =
      std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  if (length > 0.f) {
    dir[0] /= length;
    dir[1] /= length;
    dir[2] /= length;
  }
}

} // namespace

int main(int argc, const char **argv)
{
  const Options options = parseOptions(argc, argv);

  ANARILibrary library =
      anariLoadLibrary(options.libraryName, statusCallback, nullptr);
  if (!library) {
    std::fprintf(
        stderr, "Failed to load ANARI library '%s'\n", options.libraryName);
    return 1;
  }

  ANARIDevice device = anariNewDevice(library, options.deviceName);
  if (!device) {
    std::fprintf(
        stderr, "Failed to create ANARI device '%s'\n", options.deviceName);
    return 1;
  }

  // Create box geometry with per-vertex colors.
  // clang-format off
  const float boxVertex[] = {
      -1.f, 0.f, -1.f,
      1.f, 0.f, -1.f,
      1.f, 0.f, 1.f,
      -1.f, 0.f, 1.f,
      -1.f, 2.f, -1.f,
      1.f, 2.f, -1.f,
      1.f, 2.f, 1.f,
      -1.f, 2.f, 1.f,
  };
  const float boxColor[] = {
      0.9f, 0.22f, 0.18f, 1.f,
      0.95f, 0.35f, 0.18f, 1.f,
      0.95f, 0.45f, 0.24f, 1.f,
      0.9f, 0.28f, 0.20f, 1.f,
      1.f, 0.70f, 0.26f, 1.f,
      1.f, 0.82f, 0.34f, 1.f,
      1.f, 0.72f, 0.32f, 1.f,
      1.f, 0.60f, 0.28f, 1.f,
  };
  const uint32_t boxIndex[] = {
      0, 4, 5,
      0, 5, 1,
      1, 5, 6,
      1, 6, 2,
      2, 6, 7,
      2, 7, 3,
      3, 7, 4,
      3, 4, 0,
      4, 7, 6,
      4, 6, 5,
      0, 1, 2,
      0, 2, 3,
  };
  // clang-format on

  ANARIGeometry boxGeometry = anariNewGeometry(device, "triangle");
  setParameterArray1D(
      device, boxGeometry, "vertex.position", ANARI_FLOAT32_VEC3, boxVertex, 8);
  setParameterArray1D(
      device, boxGeometry, "vertex.color", ANARI_FLOAT32_VEC4, boxColor, 8);
  setParameterArray1D(
      device, boxGeometry, "primitive.index", ANARI_UINT32_VEC3, boxIndex, 12);
  anariCommitParameters(device, boxGeometry);

  // Create floor geometry with a constant material color.
  // clang-format off
  const float floorVertex[] = {
      -8.f, 0.f, -8.f,
      8.f, 0.f, -8.f,
      8.f, 0.f, 8.f,
      -8.f, 0.f, 8.f,
  };
  const uint32_t floorIndex[] = {
      0, 2, 1,
      0, 3, 2,
  };
  // clang-format on

  ANARIGeometry floorGeometry = anariNewGeometry(device, "triangle");
  setParameterArray1D(device,
      floorGeometry,
      "vertex.position",
      ANARI_FLOAT32_VEC3,
      floorVertex,
      4);
  setParameterArray1D(device,
      floorGeometry,
      "primitive.index",
      ANARI_UINT32_VEC3,
      floorIndex,
      2);
  anariCommitParameters(device, floorGeometry);

  // Create one matte material that reads vertex color, and one constant color.
  ANARIMaterial boxMaterial = anariNewMaterial(device, "matte");
  ANARIMaterial floorMaterial = anariNewMaterial(device, "matte");
  anariSetParameter(device, boxMaterial, "color", ANARI_STRING, "color");
  anariCommitParameters(device, boxMaterial);
  const float floorColor[3] = {0.35f, 0.45f, 0.38f};
  anariSetParameter(
      device, floorMaterial, "color", ANARI_FLOAT32_VEC3, &floorColor[0]);
  anariCommitParameters(device, floorMaterial);

  // Connect each geometry/material pair through its own surface.
  ANARISurface boxSurface = anariNewSurface(device);
  ANARISurface floorSurface = anariNewSurface(device);
  anariSetParameter(
      device, boxSurface, "geometry", ANARI_GEOMETRY, &boxGeometry);
  anariSetParameter(
      device, boxSurface, "material", ANARI_MATERIAL, &boxMaterial);
  anariCommitParameters(device, boxSurface);
  anariRelease(device, boxGeometry);
  anariRelease(device, boxMaterial);

  anariSetParameter(
      device, floorSurface, "geometry", ANARI_GEOMETRY, &floorGeometry);
  anariSetParameter(
      device, floorSurface, "material", ANARI_MATERIAL, &floorMaterial);
  anariCommitParameters(device, floorSurface);
  anariRelease(device, floorGeometry);
  anariRelease(device, floorMaterial);

  // Attach both surfaces to the world.
  ANARIWorld world = anariNewWorld(device);
  ANARISurface surfaces[] = {boxSurface, floorSurface};
  setParameterArray1D(device, world, "surface", ANARI_SURFACE, surfaces, 2);
  anariCommitParameters(device, world);
  anariRelease(device, boxSurface);
  anariRelease(device, floorSurface);

  // Create a hard-coded camera with a three-quarter view of the scene.
  ANARICamera camera = anariNewCamera(device, "perspective");
  const float aspect = float(WIDTH) / float(HEIGHT);
  const float position[3] = {4.5f, 3.f, 6.f};
  const float target[3] = {0.f, 0.9f, 0.f};
  float direction[3] = {};
  normalizeDirection(position, target, direction);
  const float up[3] = {0.f, 1.f, 0.f};
  anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
  anariSetParameter(
      device, camera, "position", ANARI_FLOAT32_VEC3, &position[0]);
  anariSetParameter(
      device, camera, "direction", ANARI_FLOAT32_VEC3, &direction[0]);
  anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, &up[0]);
  anariCommitParameters(device, camera);

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

  anariRelease(device, renderer);
  anariRelease(device, camera);
  anariRelease(device, world);

  // Render, map the color channel, and write the pixels to PNG.
  anariRenderFrame(device, frame);
  anariFrameReady(device, frame, ANARI_WAIT);

  uint32_t width = 0;
  uint32_t height = 0;
  ANARIDataType pixelType = ANARI_UNKNOWN;
  const void *mappedFrame = anariMapFrame(
      device, frame, "channel.color", &width, &height, &pixelType);

  bool ok = mappedFrame && pixelType == ANARI_UFIXED8_RGBA_SRGB;
  if (ok) {
    stbi_flip_vertically_on_write(1);
    ok = stbi_write_png(options.outputFile,
             int(width),
             int(height),
             4,
             mappedFrame,
             int(width) * 4)
        != 0;
  }
  if (mappedFrame)
    anariUnmapFrame(device, frame, "channel.color");

  anariRelease(device, frame);
  anariRelease(device, device);
  anariUnloadLibrary(library);

  if (!ok) {
    std::fprintf(stderr, "Failed to write '%s'\n", options.outputFile);
    return 1;
  }

  std::printf("Wrote %s\n", options.outputFile);
  return 0;
}
