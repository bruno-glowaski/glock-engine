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
#include "swapchain.hpp"
#include "window.hpp"

const size_t kMaxConcurrentFrames = 2;
const auto kVertShaderPath = "./assets/shaders/white.vert.spv";
const auto kFragShaderPath = "./assets/shaders/white.frag.spv";

vk::UniqueRenderPass createRenderPass(vk::Device device,
                                      vk::Format swapchainFormat) {
  auto attachments = std::to_array({
      vk::AttachmentDescription{}
          .setFormat(swapchainFormat)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
  });
  auto attachmentsRefs = std::to_array({
      vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal},
  });
  auto subpasses = std::to_array({
      vk::SubpassDescription{}
          .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
          .setColorAttachments(attachmentsRefs[0]),
  });
  auto subpassDependencies = std::to_array({
      vk::SubpassDependency{}
          .setSrcSubpass(vk::SubpassExternal)
          .setDstSubpass(0)
          .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
  });
  return device.createRenderPassUnique(vk::RenderPassCreateInfo{
      {}, attachments, subpasses, subpassDependencies});
}

struct RenderContext {
  Swapchain swapchain;
  vk::UniqueRenderPass renderPass;
  std::vector<vk::UniqueFramebuffer> framebuffers;
  std::vector<vk::CommandBuffer> commandBuffers;

  inline size_t imageCount() const { return swapchain.imageCount(); }
  inline vk::Extent2D extent() const { return swapchain.extent(); }
  inline bool needsRecreation() const { return swapchain.needsRecreation(); }
};

RenderContext createRenderContext(const Window &window,
                                  const GraphicsDevice &device,
                                  vk::CommandPool commandPool,
                                  std::optional<Swapchain> oldSwapchain = {}) {
  auto swapchain = Swapchain::create(window, device, std::move(oldSwapchain));
  auto renderPass = createRenderPass(device.vkDevice(), swapchain.format());
  auto framebuffers = swapchain.images() |
                      views::transform([&](vk::ImageView view) {
                        return device.vkDevice().createFramebufferUnique(
                            vk::FramebufferCreateInfo{}
                                .setRenderPass(*renderPass)
                                .setAttachments(view)
                                .setWidth(swapchain.extent().width)
                                .setHeight(swapchain.extent().height)
                                .setLayers(1));
                      }) |
                      std::ranges::to<std::vector>();
  auto commandBuffers = device.vkDevice().allocateCommandBuffers(
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount((uint32_t)framebuffers.size()));
  return {.swapchain = std::move(swapchain),
          .renderPass = std::move(renderPass),
          .framebuffers = std::move(framebuffers),
          .commandBuffers = commandBuffers};
}

vk::UniqueShaderModule loadShaderFromPath(const std::string_view path,
                                          vk::Device device) {
  std::ifstream file(path.data(), std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("failed to open file {}", path));
  }
  std::vector<char> shaderData{std::istreambuf_iterator(file), {}};
  return device.createShaderModuleUnique(
      vk::ShaderModuleCreateInfo{}
          .setCodeSize(shaderData.size())
          .setPCode(reinterpret_cast<const uint32_t *>(shaderData.data())));
}

std::tuple<vk::UniquePipeline, vk::UniquePipelineLayout,
           std::array<vk::UniqueShaderModule, 2>>
createWhiteGraphicsPipeline(vk::Extent2D swapchainExtent,
                            vk::RenderPass renderPass, vk::Device device) {
  auto shaderModules = std::to_array({
      loadShaderFromPath(kVertShaderPath, device),
      loadShaderFromPath(kFragShaderPath, device),
  });
  auto pipelineLayout =
      device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{});
  auto pipelineStages = std::to_array({
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eVertex, *shaderModules[0], "main"},
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eFragment, *shaderModules[1], "main"},
  });
  auto vertexBindings = std::to_array({
      vk::VertexInputBindingDescription{0, sizeof(glm::vec4)},
  });
  auto vertexAttributes = std::to_array({
      vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat,
                                          0},
  });
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      {}, vertexBindings, vertexAttributes};
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
      {}, vk::PrimitiveTopology::eTriangleList};
  vk::Viewport viewport{
      0.0f,
      0.0f,
      static_cast<float>(swapchainExtent.width),
      static_cast<float>(swapchainExtent.height),
      0.0f,
      1.0f,
  };
  vk::Rect2D scissor{{0, 0}, swapchainExtent};
  vk::PipelineViewportStateCreateInfo viewportInfo{{}, viewport, scissor};
  auto rasterizationState =
      vk::PipelineRasterizationStateCreateInfo{}.setLineWidth(1.0f);
  vk::PipelineMultisampleStateCreateInfo multisampleState{};
  auto colorBlendingAttachments = std::to_array({
      vk::PipelineColorBlendAttachmentState{}
          .setBlendEnable(false)
          .setColorWriteMask(
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
  });
  auto colorBlendState = vk::PipelineColorBlendStateCreateInfo{}.setAttachments(
      colorBlendingAttachments);
  auto pipeline = device
                      .createGraphicsPipelineUnique(
                          {}, vk::GraphicsPipelineCreateInfo{}
                                  .setStages(pipelineStages)
                                  .setPVertexInputState(&vertexInputInfo)
                                  .setPInputAssemblyState(&inputAssemblyInfo)
                                  .setPViewportState(&viewportInfo)
                                  .setPRasterizationState(&rasterizationState)
                                  .setPMultisampleState(&multisampleState)
                                  .setPColorBlendState(&colorBlendState)
                                  .setLayout(*pipelineLayout)
                                  .setRenderPass(renderPass))
                      .value;
  return std::make_tuple(std::move(pipeline), std::move(pipelineLayout),
                         std::move(shaderModules));
}

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

void recordRenderCommandBuffer(vk::CommandBuffer commandBuffer,
                               vk::Extent2D extent, vk::RenderPass renderPass,
                               vk::Pipeline pipeline, vk::Buffer vertexBuffer,
                               uint32_t verticeCount,
                               vk::Framebuffer framebuffer) {
  commandBuffer.begin(vk::CommandBufferBeginInfo{});
  vk::ClearValue clearValues = {
      vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
  commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{}
                                    .setRenderPass(renderPass)
                                    .setFramebuffer(framebuffer)
                                    .setClearValues(clearValues)
                                    .setRenderArea(vk::Rect2D{{0, 0}, extent}),
                                vk::SubpassContents::eInline);
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
  commandBuffer.bindVertexBuffers(0, vertexBuffer, {0});
  commandBuffer.draw(verticeCount, 1, 0, 0);
  commandBuffer.endRenderPass();
  commandBuffer.end();
}

void render(std::span<vk::CommandBuffer> commandBuffers,
            vk::Queue graphicsQueue, Swapchain &swapchain) {
  auto frame = swapchain.nextImage();
  if (!frame.has_value()) {
    return;
  }
  auto commandBuffer = commandBuffers[frame->image];
  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  auto submitInfo = vk::SubmitInfo{}
                        .setWaitSemaphores(frame->readySemaphore)
                        .setWaitDstStageMask(waitStage)
                        .setCommandBuffers(commandBuffer)
                        .setSignalSemaphores(frame->doneSemaphore);
  graphicsQueue.submit(submitInfo, frame->fence);
  swapchain.present(*frame);
}

int main(void) {
  auto window = Window::create("Glock Engine", 640, 480);
  auto device =
      GraphicsDevice::createFor(window, "Glock Engine", MAKE_VERSION(0, 1, 0));
  auto workCommandPool =
      device.vkDevice().createCommandPoolUnique(vk::CommandPoolCreateInfo{
          vk::CommandPoolCreateFlagBits::eTransient, device.workQueueIndex()});
  auto renderCommandPool = device.vkDevice().createCommandPoolUnique(
      vk::CommandPoolCreateInfo{{}, device.graphicsQueueIndex()});
  auto renderContext = createRenderContext(window, device, *renderCommandPool);
  auto [pipeline, pipelineLayout, shaderModules] = createWhiteGraphicsPipeline(
      renderContext.extent(), *renderContext.renderPass, device.vkDevice());

  // Load vertex buffer
  auto vertices = std::to_array({glm::vec4(-1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(0.0, -1.0, 0.0, 1.0)});
  auto [vertexBuffer, vertexBufferAllocation] = createImmutableBuffer(
      vertices, vk::BufferUsageFlagBits::eVertexBuffer, *workCommandPool,
      device.workQueue(), device.vkDevice(), device.vmaAllocator());

  // Record command buffers.
  for (auto [commandBuffer, framebuffer] :
       views::zip(renderContext.commandBuffers, renderContext.framebuffers)) {
    recordRenderCommandBuffer(commandBuffer, renderContext.extent(),
                              *renderContext.renderPass, *pipeline,
                              *vertexBuffer, vertices.size(), *framebuffer);
  }

  // Game loop
  while (!window.shouldClose()) {
    render(renderContext.commandBuffers, device.graphicsQueue(),
           renderContext.swapchain);
    if (renderContext.needsRecreation()) {
      window.waitForValidDimensions();
      device.waitIdle();
      renderContext = createRenderContext(window, device, *renderCommandPool,
                                          std::move(renderContext.swapchain));
    }
    glfwPollEvents();
  }
  device.waitIdle();
  return 0;
}
