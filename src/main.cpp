#include "material.hpp"
#include "materials/procedural.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <glm/ext/matrix_transform.hpp>
#include <print>
#include <ratio>
#include <string_view>

#include <thread>
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "buffer.hpp"
#include "buffer_impl.hpp"
#include "graphics_device.hpp"
#include "materials/colorful.hpp"
#include "materials/procedural.hpp"
#include "model.hpp"
#include "model_impl.hpp"
#include "render_system.hpp"
#include "swapchain.hpp"
#include "window.hpp"

using Second = std::chrono::duration<uint64_t>;
using FSecond = std::chrono::duration<float>;
using FrameDuration = std::chrono::duration<uint64_t, std::nano>;
using FFrameDuration = std::chrono::duration<float>;

const uint64_t MAX_FPS = 60;
const FrameDuration MIN_FRAME_DURATION =
    std::chrono::duration_cast<FrameDuration>(Second{1}) / MAX_FPS;
const auto kModelVertices =
    std::to_array<ColorfulMaterial::Vertex>({{1.0, 1.0, -1.0},
                                             {1.0, -1.0, -1.0},
                                             {1.0, 1.0, 1.0},
                                             {1.0, -1.0, 1.0},
                                             {-1.0, 1.0, -1.0},
                                             {-1.0, -1.0, -1.0},
                                             {-1.0, 1.0, 1.0},
                                             {-1.0, -1.0, 1.0}});
const auto kModelIndices = std::to_array<uint16_t>(
    {4, 2, 0, 2, 7, 3, 6, 5, 7, 1, 7, 5, 0, 3, 1, 4, 1, 5,
     4, 6, 2, 2, 6, 7, 6, 4, 5, 1, 3, 7, 0, 2, 3, 4, 0, 1});
const auto kSkyBoxVertices =
    std::to_array<ProceduralMaterial::Vertex>({{50.0, 50.0, -50.0},
                                               {50.0, -50.0, -50.0},
                                               {50.0, 50.0, 50.0},
                                               {50.0, -50.0, 50.0},
                                               {-50.0, 50.0, -50.0},
                                               {-50.0, -50.0, -50.0},
                                               {-50.0, 50.0, 50.0},
                                               {-50.0, -50.0, 50.0}});
const auto kSkyBoxIndices = std::to_array<uint16_t>(
    {0, 4, 2, 3, 2, 7, 7, 6, 5, 5, 1, 7, 1, 0, 3, 5, 4, 1,
     2, 4, 6, 7, 2, 6, 5, 6, 4, 7, 1, 3, 3, 0, 2, 1, 4, 0});

template <std::invocable<FrameDuration> TTick>
inline void runGameLoop(const Window &window, TTick tick) {
  auto lastTime = std::chrono::system_clock::now();
  FrameDuration totalTime{};
  FrameDuration lag{};
  while (!window.shouldClose()) {
    auto currentTime = std::chrono::system_clock::now();
    auto deltaTime =
        std::chrono::duration_cast<FrameDuration>(currentTime - lastTime);
    lastTime = currentTime;
    totalTime += deltaTime;
    lag += deltaTime;

    glfwPollEvents();

    if (lag < MIN_FRAME_DURATION) {
      auto d = MIN_FRAME_DURATION - lag;
      std::this_thread::sleep_for(d);
      continue;
    }
    lag -= MIN_FRAME_DURATION;

    tick(totalTime);
  }
}

int main(void) {
  auto window = Window::create("Glock Engine", 640, 480);
  auto device =
      GraphicsDevice::createFor(window, "Glock Engine", MAKE_VERSION(0, 1, 0));
  auto swapchain = Swapchain::create(window, device);

  // Create systems
  auto renderSystem = RenderSystem::create(device, swapchain);

  // Load model
  auto material = ColorfulMaterial::create(device, renderSystem);
  auto model = Model::fromRanges(device, kModelVertices, kModelIndices);

  // Load skybox
  auto skyBoxMaterial = ProceduralMaterial::create(
      device, renderSystem, "./assets/shaders/vaporwave_skybox.vert.spv",
      "./assets/shaders/vaporwave_skybox.frag.spv");
  auto skyBox = Model::fromRanges(device, kSkyBoxVertices, kSkyBoxIndices);

  runGameLoop(window, [&](FrameDuration totalTime) {
    material.setTime(totalTime);

    auto viewport = swapchain.extent();
    const float fov = glm::radians(90.0f);
    const float aspectRatio = static_cast<float>(viewport.width) /
                              static_cast<float>(viewport.height);
    auto seconds = std::chrono::duration_cast<FSecond>(totalTime).count();
    auto modelMat = glm::rotate(
        glm::translate(
            glm::identity<glm::mat4>(),
            glm::vec3{0.0, 7.0 + 6.0 * glm::sin(seconds * 1.5), 12.0}),
        seconds, glm::vec3{1.0, 2.0, 3.0});
    auto viewMat =
        glm::lookAt(glm::vec3{0.0, 0.0, 0.0}, glm::vec3{0.0, 0.5, 1.0},
                    glm::vec3{0.0, 1.0, 0.0});
    auto projMat = glm::perspective(fov, aspectRatio, 0.1f, 100.0f);
    projMat[1][1] *= -1;
    auto modelMvp = projMat * viewMat * modelMat;
    auto skyBoxMvp = projMat * viewMat;

    if (swapchain.needsRecreation()) {
      window.waitForValidDimensions();
      device.waitIdle();
      swapchain = Swapchain::create(window, device, std::move(swapchain));
      renderSystem =
          RenderSystem::create(device, swapchain, std::move(renderSystem));
      material =
          ColorfulMaterial::create(device, renderSystem, std::move(material));
      skyBoxMaterial = ProceduralMaterial::create(
          device, renderSystem, "./assets/shaders/vaporwave_skybox.vert.spv",
          "./assets/shaders/vaporwave_skybox.frag.spv",
          std::move(skyBoxMaterial));
    }

    auto frame = swapchain.nextImage();
    if (!frame.has_value()) {
      return;
    }
    renderSystem.render(*frame, viewport,
                        {
                            {material, {modelMvp}, model},
                            {skyBoxMaterial, {skyBoxMvp}, skyBox},
                        });
    swapchain.present(*frame);
  });
  device.waitIdle();
  return 0;
}
