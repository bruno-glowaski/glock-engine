#include <array>
#include <cstdlib>
#include <print>
#include <string_view>

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "buffer.hpp"
#include "buffer_impl.hpp"
#include "graphics_device.hpp"
#include "render_system.hpp"
#include "swapchain.hpp"
#include "window.hpp"

const auto kVertices =
    std::to_array({glm::vec3(-1.0, 1.0, 0.0), glm::vec3(1.0, 1.0, 0.0),
                   glm::vec3(0.0, -1.0, 0.0)});
const auto kIndices = std::to_array<uint16_t>({0, 1, 2});

int main(void) {
  auto window = Window::create("Glock Engine", 640, 480);
  auto device =
      GraphicsDevice::createFor(window, "Glock Engine", MAKE_VERSION(0, 1, 0));
  auto workCommandPool =
      device.createWorkCommandPool(vk::CommandPoolCreateFlagBits::eTransient);
  auto swapchain = Swapchain::create(window, device);

  // Load model
  auto vertexBuffer =
      Buffer::createGPUOnlyArray(device, workCommandPool.get(), kVertices,
                                 vk::BufferUsageFlagBits::eVertexBuffer);
  auto indexBuffer =
      Buffer::createGPUOnlyArray(device, workCommandPool.get(), kIndices,
                                 vk::BufferUsageFlagBits::eIndexBuffer);

  // Create systems
  auto renderSystem = RenderSystem::create(device, swapchain);

  // Game loop
  while (!window.shouldClose()) {
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
    }
    glfwPollEvents();
  }
  device.waitIdle();
  return 0;
}
