#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace views = std::ranges::views;

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics_device.hpp"
#include "render_system.hpp"
#include "swapchain.hpp"
#include "window.hpp"

template <class T, size_t Nm>
std::tuple<vma::UniqueBuffer, vma::UniqueAllocation>
createImmutableBuffer(const std::array<T, Nm> &data, vk::BufferUsageFlags usage,
                      vk::CommandPool commandPool, vk::Queue queue,
                      vk::Device device, vma::Allocator allocator) {
  const size_t size = sizeof(data);

  auto finalBuffer = allocator.createBufferUnique(
      vk::BufferCreateInfo{}
          .setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
          .setSize(size),
      vma::AllocationCreateInfo{}.setUsage(vma::MemoryUsage::eGpuOnly));

  auto stagingBuffer = allocator.createBufferUnique(
      vk::BufferCreateInfo{}
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSize(size),
      vma::AllocationCreateInfo{}.setUsage(vma::MemoryUsage::eCpuToGpu));
  T *stagingPtr =
      reinterpret_cast<T *>(allocator.mapMemory(*stagingBuffer.second));
  std::ranges::copy(data, stagingPtr);
  allocator.unmapMemory(*stagingBuffer.second);

  auto commandBuffer =
      device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{}
                                        .setCommandPool(commandPool)
                                        .setCommandBufferCount(1))[0];
  commandBuffer.begin(vk::CommandBufferBeginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  commandBuffer.copyBuffer(*stagingBuffer.first, *finalBuffer.first,
                           vk::BufferCopy{}.setSize(size));
  commandBuffer.end();
  queue.submit(vk::SubmitInfo{}.setCommandBuffers(commandBuffer));
  queue.waitIdle();
  device.freeCommandBuffers(commandPool, commandBuffer);

  return std::make_tuple(std::move(finalBuffer.first),
                         std::move(finalBuffer.second));
}

int main(void) {
  auto window = Window::create("Glock Engine", 640, 480);
  auto device =
      GraphicsDevice::createFor(window, "Glock Engine", MAKE_VERSION(0, 1, 0));
  auto workCommandPool =
      device.vkDevice().createCommandPoolUnique(vk::CommandPoolCreateInfo{
          vk::CommandPoolCreateFlagBits::eTransient, device.workQueueIndex()});
  auto swapchain = Swapchain::create(window, device);

  // Load vertex buffer
  auto vertices = std::to_array({glm::vec4(-1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(0.0, -1.0, 0.0, 1.0)});
  auto [vertexBuffer, vertexBufferAllocation] = createImmutableBuffer(
      vertices, vk::BufferUsageFlagBits::eVertexBuffer, *workCommandPool,
      device.workQueue(), device.vkDevice(), device.vmaAllocator());

  // Create systems
  auto renderSystem = RenderSystem::create(device, swapchain,
                                           vertexBuffer.get(), vertices.size());

  // Game loop
  while (!window.shouldClose()) {
    auto frame = swapchain.nextImage();
    if (frame.has_value()) {
      renderSystem.render(*frame);
      swapchain.present(*frame);
    }
    if (swapchain.needsRecreation()) {
      window.waitForValidDimensions();
      device.waitIdle();
      swapchain = Swapchain::create(window, device, std::move(swapchain));
      renderSystem =
          RenderSystem::create(device, swapchain, vertexBuffer.get(),
                               vertices.size(), std::move(renderSystem));
    }
    glfwPollEvents();
  }
  device.waitIdle();
  return 0;
}
