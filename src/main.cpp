#include <array>
#include <chrono>
#include <cstdlib>
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

const auto kVertices =
    std::to_array<ColorfulMaterial::Vertex>({{glm::vec3(-1.0, 1.0, 0.0)},
                                             {glm::vec3(1.0, 1.0, 0.0)},
                                             {glm::vec3(0.0, -1.0, 0.0)}});
const auto kIndices = std::to_array<uint16_t>({0, 1, 2});

int main(void) {
  auto window = Window::create("Glock Engine", 640, 480);
  auto device =
      GraphicsDevice::createFor(window, "Glock Engine", MAKE_VERSION(0, 1, 0));
  auto workCommandPool =
      device.createWorkCommandPool(vk::CommandPoolCreateFlagBits::eTransient);
  auto swapchain = Swapchain::create(window, device);

  // Create systems
  auto renderSystem = RenderSystem::create(device, swapchain);

  // Load model
  auto material = ColorfulMaterial::create(device, renderSystem);
  renderSystem.setMaterial(material);
  auto vertexBuffer =
      Buffer::createGPUOnlyArray(device, workCommandPool.get(), kVertices,
                                 vk::BufferUsageFlagBits::eVertexBuffer);
  auto indexBuffer =
      Buffer::createGPUOnlyArray(device, workCommandPool.get(), kIndices,
                                 vk::BufferUsageFlagBits::eIndexBuffer);

  // Game loop
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

    material.setTime(totalTime);

    auto frame = swapchain.nextImage();
    if (frame.has_value()) {
      renderSystem.render(*frame, swapchain.extent(), vertexBuffer.vkBuffer(),
                          indexBuffer.vkBuffer(), kIndices.size());
      swapchain.present(*frame);
    }
    if (swapchain.needsRecreation()) {
      window.waitForValidDimensions();
      device.waitIdle();
      swapchain = Swapchain::create(window, device, std::move(swapchain));
      renderSystem =
          RenderSystem::create(device, swapchain, std::move(renderSystem));
      material =
          ColorfulMaterial::create(device, renderSystem, std::move(material));
    }
  }
  device.waitIdle();
  return 0;
}
