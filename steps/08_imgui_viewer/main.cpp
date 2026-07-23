// Copyright 2026 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include <anari/anari.h>

#include "Orbit.h"
#include "import_gltf.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t DEFAULT_WIDTH = 1200;
constexpr uint32_t DEFAULT_HEIGHT = 800;

struct Options
{
  const char *filename = "scene.gltf";
  const char *libraryName = "environment";
  const char *deviceName = "default";
};

struct UiState
{
  SDL_Texture *colorTexture = nullptr;
  std::vector<uint8_t> pixels;
  uint32_t frameSize[2] = {DEFAULT_WIDTH, DEFAULT_HEIGHT};
  bool frameInFlight = false;
  double previousMouseX = 0.0;
  double previousMouseY = 0.0;
  bool hasPreviousMouse = false;
  bool mouseRotating = false;
  float ambientRadiance = 1.f;
  float background[4] = {0.02f, 0.025f, 0.03f, 1.f};
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

bool updateCamera(ANARIDevice device,
    ANARICamera camera,
    const anari_viewer::manipulators::Orbit &orbit,
    const UiState &state)
{
  const anari::math::float3 position = orbit.eye();
  const anari::math::float3 direction = orbit.dir();
  const anari::math::float3 up = orbit.up();
  const float aspect = float(state.frameSize[0]) / float(state.frameSize[1]);

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
    anari_viewer::manipulators::Orbit &orbit,
    const UiState &state)
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
  updateCamera(device, camera, orbit, state);
}

uint8_t toSdlColor(float c)
{
  return uint8_t(std::clamp(c, 0.f, 1.f) * 255.f + 0.5f);
}

bool createTexture(SDL_Renderer *renderer, UiState &state)
{
  if (state.colorTexture)
    SDL_DestroyTexture(state.colorTexture);

  state.colorTexture = SDL_CreateTexture(renderer,
      SDL_PIXELFORMAT_RGBA32,
      SDL_TEXTUREACCESS_STREAMING,
      int(state.frameSize[0]),
      int(state.frameSize[1]));
  if (!state.colorTexture) {
    std::fprintf(stderr, "Failed to create SDL texture: %s\n", SDL_GetError());
    return false;
  }

  SDL_SetTextureScaleMode(state.colorTexture, SDL_SCALEMODE_LINEAR);
  return true;
}

bool resizeFrameAndTexture(ANARIDevice device,
    ANARIFrame frame,
    ANARICamera camera,
    const anari_viewer::manipulators::Orbit &orbit,
    UiState &state,
    SDL_Renderer *renderer,
    int framebufferWidth,
    int framebufferHeight)
{
  state.frameSize[0] = uint32_t(std::max(framebufferWidth, 1));
  state.frameSize[1] = uint32_t(std::max(framebufferHeight, 1));
  state.pixels.resize(size_t(state.frameSize[0]) * state.frameSize[1] * 4);

  anariSetParameter(
      device, frame, "size", ANARI_UINT32_VEC2, &state.frameSize[0]);
  anariCommitParameters(device, frame);
  updateCamera(device, camera, orbit, state);

  return createTexture(renderer, state);
}

bool mapFrameToPixels(ANARIDevice device, ANARIFrame frame, UiState &state)
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
  const uint32_t copyWidth = std::min(width, state.frameSize[0]);
  const uint32_t copyHeight = std::min(height, state.frameSize[1]);
  const size_t dstRowBytes = size_t(state.frameSize[0]) * 4;
  const size_t srcRowBytes = size_t(width) * 4;
  const size_t copyRowBytes = size_t(copyWidth) * 4;
  for (uint32_t y = 0; y < copyHeight; ++y) {
    const uint32_t srcY = height - 1 - y;
    std::memcpy(state.pixels.data() + size_t(y) * dstRowBytes,
        srcPixels + size_t(srcY) * srcRowBytes,
        copyRowBytes);
  }

  anariUnmapFrame(device, frame, "channel.color");
  return true;
}

bool uploadFrameTexture(const UiState &state)
{
  if (!SDL_UpdateTexture(state.colorTexture,
          nullptr,
          state.pixels.data(),
          int(state.frameSize[0]) * 4)) {
    std::fprintf(stderr, "Failed to update SDL texture: %s\n", SDL_GetError());
    return false;
  }
  return true;
}

void handleCameraInput(SDL_Window *window,
    anari_viewer::manipulators::Orbit &orbit,
    UiState &state)
{
  ImGuiIO &io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    state.mouseRotating = false;
    return;
  }

  float mouseX = 0.f;
  float mouseY = 0.f;
  const auto buttons = SDL_GetMouseState(&mouseX, &mouseY);
  const bool rotate = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
  const bool dolly = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
  const bool pan = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0;

  if (!state.hasPreviousMouse) {
    state.previousMouseX = mouseX;
    state.previousMouseY = mouseY;
    state.hasPreviousMouse = true;
    return;
  }

  const anari::math::float2 mouseFrom{
      float(state.previousMouseX), float(state.previousMouseY)};
  const anari::math::float2 mouseTo{float(mouseX), float(mouseY)};
  state.previousMouseX = mouseX;
  state.previousMouseY = mouseY;

  if (!rotate && !dolly && !pan) {
    state.mouseRotating = false;
    return;
  }

  int windowWidth = 1;
  int windowHeight = 1;
  SDL_GetWindowSize(window, &windowWidth, &windowHeight);
  const anari::math::float2 viewportSize{
      float(std::max(windowWidth, 1)), float(std::max(windowHeight, 1))};
  const anari::math::float2 mouseDelta =
      (mouseTo * 2.f / viewportSize) - (mouseFrom * 2.f / viewportSize);
  if (mouseDelta == anari::math::float2(0.f))
    return;

  if (dolly) {
    orbit.zoom(mouseDelta.y);
    state.mouseRotating = false;
  } else if (pan) {
    orbit.pan(mouseDelta);
    state.mouseRotating = false;
  } else if (rotate) {
    if (!state.mouseRotating) {
      orbit.startNewRotation();
      state.mouseRotating = true;
    }
    orbit.rotate(anari::math::float2(-mouseDelta.x, -mouseDelta.y));
  }
}

void drawSceneImage(SDL_Texture *texture)
{
  if (!texture)
    return;

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  const ImVec2 min = viewport->WorkPos;
  const ImVec2 max(viewport->WorkPos.x + viewport->WorkSize.x,
      viewport->WorkPos.y + viewport->WorkSize.y);
  ImGui::GetBackgroundDrawList(viewport)->AddImage(
      reinterpret_cast<ImTextureID>(texture), min, max);
}

void drawControls(ANARIDevice device,
    ANARIRenderer renderer,
    ANARIWorld world,
    ANARICamera camera,
    anari_viewer::manipulators::Orbit &orbit,
    UiState &state)
{
  ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(320.f, 0.f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("ANARI Viewer")) {
    ImGui::End();
    return;
  }

  if (ImGui::Button("Reset Camera"))
    resetCameraFromWorldBounds(device, world, camera, orbit, state);

  bool rendererChanged = false;
  rendererChanged |=
      ImGui::SliderFloat("Ambient Radiance", &state.ambientRadiance, 0.f, 3.f);
  rendererChanged |= ImGui::ColorEdit4("Background", state.background);
  if (rendererChanged) {
    anariSetParameter(device,
        renderer,
        "ambientRadiance",
        ANARI_FLOAT32,
        &state.ambientRadiance);
    anariSetParameter(device,
        renderer,
        "background",
        ANARI_FLOAT32_VEC4,
        &state.background[0]);
    anariCommitParameters(device, renderer);
  }

  const anari::math::float3 eye = orbit.eye();
  const anari::math::float3 at = orbit.at();
  ImGui::Separator();
  ImGui::Text("Frame: %u x %u", state.frameSize[0], state.frameSize[1]);
  ImGui::Text("Eye: %.2f, %.2f, %.2f", eye.x, eye.y, eye.z);
  ImGui::Text("At: %.2f, %.2f, %.2f", at.x, at.y, at.z);
  ImGui::Text("LMB orbit, MMB pan, RMB dolly");

  ImGui::End();
}

} // namespace

int main(int argc, const char **argv)
{
  const Options options = parseOptions(argc, argv);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow("ANARI Course 08 - ImGui Viewer",
      int(DEFAULT_WIDTH),
      int(DEFAULT_HEIGHT),
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    std::fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *sdlRenderer = SDL_CreateRenderer(window, nullptr);
  if (!sdlRenderer) {
    std::fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_SetRenderVSync(sdlRenderer, 1);
  // Set render scale to match window pixel density for crisp rendering on
  // high-DPI displays.
  const float pixelDensity = SDL_GetWindowPixelDensity(window);
  SDL_SetRenderScale(sdlRenderer, pixelDensity, pixelDensity);

  ANARILibrary library =
      anariLoadLibrary(options.libraryName, statusCallback, nullptr);
  if (!library) {
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  ANARIDevice device = anariNewDevice(library, options.deviceName);
  if (!device) {
    anariUnloadLibrary(library);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  anariCommitParameters(device, device);

  ANARIWorld world = import_gltf(device, options.filename);
  if (!world) {
    anariRelease(device, device);
    anariUnloadLibrary(library);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  ANARICamera camera = anariNewCamera(device, "perspective");
  ANARIRenderer renderer = anariNewRenderer(device, "default");
  ANARIFrame frame = anariNewFrame(device);
  if (!camera || !renderer || !frame) {
    anariRelease(device, device);
    anariUnloadLibrary(library);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  UiState state;
  anari_viewer::manipulators::Orbit orbit;
  resetCameraFromWorldBounds(device, world, camera, orbit, state);

  anariSetParameter(
      device, renderer, "background", ANARI_FLOAT32_VEC4, &state.background[0]);
  anariSetParameter(device,
      renderer,
      "ambientRadiance",
      ANARI_FLOAT32,
      &state.ambientRadiance);
  anariCommitParameters(device, renderer);

  ANARIDataType colorFormat = ANARI_UFIXED8_RGBA_SRGB;
  anariSetParameter(
      device, frame, "channel.color", ANARI_DATA_TYPE, &colorFormat);
  anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
  anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
  anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
  anariCommitParameters(device, frame);

  int framebufferWidth = 0;
  int framebufferHeight = 0;
  SDL_GetWindowSizeInPixels(window, &framebufferWidth, &framebufferHeight);
  if (!resizeFrameAndTexture(device,
          frame,
          camera,
          orbit,
          state,
          sdlRenderer,
          framebufferWidth,
          framebufferHeight)) {
    anariRelease(device, device);
    anariUnloadLibrary(library);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().FontGlobalScale = 1.5f;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLRenderer(window, sdlRenderer);
  ImGui_ImplSDLRenderer3_Init(sdlRenderer);

  bool running = true;
  bool exitRequested = false;
  while (running && !exitRequested) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        exitRequested = true;
      } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
          && event.window.windowID == SDL_GetWindowID(window)) {
        exitRequested = true;
      } else if (event.type == SDL_EVENT_KEY_DOWN
          && event.key.key == SDLK_ESCAPE) {
        exitRequested = true;
      }
    }
    if (exitRequested)
      break;

    int currentFramebufferWidth = 0;
    int currentFramebufferHeight = 0;
    SDL_GetWindowSizeInPixels(
        window, &currentFramebufferWidth, &currentFramebufferHeight);
    if (currentFramebufferWidth != int(state.frameSize[0])
        || currentFramebufferHeight != int(state.frameSize[1])) {
      if (!state.frameInFlight) {
        running = resizeFrameAndTexture(device,
            frame,
            camera,
            orbit,
            state,
            sdlRenderer,
            currentFramebufferWidth,
            currentFramebufferHeight);
      }
    }
    if (!running)
      break;

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    handleCameraInput(window, orbit, state);
    updateCamera(device, camera, orbit, state);
    drawControls(device, renderer, world, camera, orbit, state);

    if (!state.frameInFlight) {
      anariRenderFrame(device, frame);
      state.frameInFlight = true;
    }

    if (state.frameInFlight && anariFrameReady(device, frame, ANARI_NO_WAIT)) {
      running = mapFrameToPixels(device, frame, state);
      if (running)
        running = uploadFrameTexture(state);
      state.frameInFlight = false;
    }

    drawSceneImage(state.colorTexture);

    ImGui::Render();
    SDL_SetRenderDrawColor(sdlRenderer,
        toSdlColor(state.background[0]),
        toSdlColor(state.background[1]),
        toSdlColor(state.background[2]),
        255);
    SDL_RenderClear(sdlRenderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);
    SDL_RenderPresent(sdlRenderer);
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  if (state.colorTexture)
    SDL_DestroyTexture(state.colorTexture);

  anariRelease(device, frame);
  anariRelease(device, renderer);
  anariRelease(device, camera);
  anariRelease(device, world);
  anariRelease(device, device);
  anariUnloadLibrary(library);
  SDL_DestroyRenderer(sdlRenderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return running ? 0 : 1;
}
