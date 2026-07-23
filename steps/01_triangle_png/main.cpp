// Copyright 2026 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include <anari/anari.h>
#include <anari/frontend/type_utility.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t WIDTH = 1200;
constexpr uint32_t HEIGHT = 800;

struct Options
{
  const char *outputFile = "triangle.png";
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

  // clang-format off
  // Create triangle geometry and upload vertex/index/color data.
  const float vertex[] = {
      0.f, 1.f, 0.f,
      1.f, -1.f, 0.f,
      -1.f, -1.f, 0.f,
  };
  const float color[] = {
      1.f, 0.f, 0.f, 1.f,
      0.f, 1.f, 0.f, 1.f,
      0.f, 0.f, 1.f, 1.f,
  };
  const uint32_t index[] = {0, 1, 2};
  // clang-format on

  ANARIGeometry geometry = anariNewGeometry(device, "triangle");
  setParameterArray1D(
      device, geometry, "vertex.position", ANARI_FLOAT32_VEC3, vertex, 3);
  setParameterArray1D(
      device, geometry, "vertex.color", ANARI_FLOAT32_VEC4, color, 3);
  setParameterArray1D(
      device, geometry, "primitive.index", ANARI_UINT32_VEC3, index, 1);
  anariCommitParameters(device, geometry);

  // Create a matte material that reads color from the geometry attribute.
  ANARIMaterial material = anariNewMaterial(device, "matte");
  anariSetParameter(device, material, "color", ANARI_STRING, "color");
  anariCommitParameters(device, material);

  // Connect geometry and material through a surface.
  ANARISurface surface = anariNewSurface(device);
  anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &geometry);
  anariSetParameter(device, surface, "material", ANARI_MATERIAL, &material);
  anariCommitParameters(device, surface);
  anariRelease(device, geometry);
  anariRelease(device, material);

  // Attach the surface to a world.
  ANARIWorld world = anariNewWorld(device);
  setParameterArray1D(device, world, "surface", ANARI_SURFACE, &surface, 1);
  anariCommitParameters(device, world);
  anariRelease(device, surface);

  // Create a straight-on camera for the first triangle.
  ANARICamera camera = anariNewCamera(device, "perspective");
  const float aspect = float(WIDTH) / float(HEIGHT);
  const float position[3] = {0.f, 0.f, 3.f};
  const float direction[3] = {0.f, 0.f, -1.f};
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
