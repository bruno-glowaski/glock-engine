#include "render_system.hpp"

#include <fstream>
#include <ios>
#include <vulkan/vulkan_handles.hpp>

#include "graphics_device.hpp"
#include "swapchain.hpp"

static constexpr auto kVertexBindings = std::to_array({
    vk::VertexInputBindingDescription{0, sizeof(glm::vec4)},
});
static constexpr auto kVertexAttributes = std::to_array({
    vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat,
                                        0},
});

const auto kVertShaderPath = "./assets/shaders/white.vert.spv";
const auto kFragShaderPath = "./assets/shaders/white.frag.spv";

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

vk::UniquePipeline
createSimpleGraphicsPipeline(vk::PipelineLayout layout,
                             std::span<vk::UniqueShaderModule, 2> shaderModules,
                             vk::Extent2D swapchainExtent,
                             vk::RenderPass renderPass, vk::Device device) {
  auto stages = std::to_array({
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eVertex, *shaderModules[0], "main"},
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eFragment, *shaderModules[1], "main"},
  });
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      {}, kVertexBindings, kVertexAttributes};
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
                                  .setStages(stages)
                                  .setPVertexInputState(&vertexInputInfo)
                                  .setPInputAssemblyState(&inputAssemblyInfo)
                                  .setPViewportState(&viewportInfo)
                                  .setPRasterizationState(&rasterizationState)
                                  .setPMultisampleState(&multisampleState)
                                  .setPColorBlendState(&colorBlendState)
                                  .setLayout(layout)
                                  .setRenderPass(renderPass))
                      .value;
  return pipeline;
}

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

RenderSystem RenderSystem::create(const GraphicsDevice &device,
                                  const Swapchain &swapchain,
                                  vk::Buffer vertexBuffer, uint32_t vertexCount,
                                  std::optional<RenderSystem> old) {
  auto vkDevice = device.vkDevice();

  vk::UniqueCommandPool commandPool;
  vk::UniquePipelineLayout pipelineLayout;
  std::array<vk::UniqueShaderModule, 2> shaderModules;
  if (old.has_value()) {
    commandPool = std::move(old->_vkCommandPool);
    pipelineLayout = std::move(old->_vkPipelineLayout);
    shaderModules = std::move(old->_vkShaderModules);
  } else {
    commandPool = vkDevice.createCommandPoolUnique(
        vk::CommandPoolCreateInfo{}.setQueueFamilyIndex(
            device.graphicsQueueIndex()));
    pipelineLayout =
        vkDevice.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{});
    shaderModules = std::to_array({
        loadShaderFromPath(kVertShaderPath, vkDevice),
        loadShaderFromPath(kFragShaderPath, vkDevice),
    });
  }

  auto renderPass = createRenderPass(device.vkDevice(), swapchain.format());
  auto pipeline = createSimpleGraphicsPipeline(
      pipelineLayout.get(), shaderModules, swapchain.extent(), renderPass.get(),
      vkDevice);

  auto framebuffers = swapchain.images() |
                      std::ranges::views::transform([&](vk::ImageView view) {
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
          .setCommandPool(commandPool.get())
          .setCommandBufferCount((uint32_t)framebuffers.size()));
  auto renderCommandPool = device.vkDevice().createCommandPoolUnique(
      vk::CommandPoolCreateInfo{{}, device.graphicsQueueIndex()});
  for (auto [commandBuffer, framebuffer] :
       std::ranges::views::zip(commandBuffers, framebuffers)) {
    recordRenderCommandBuffer(commandBuffer, swapchain.extent(),
                              renderPass.get(), pipeline.get(), vertexBuffer,
                              vertexCount, framebuffer.get());
  }

  return {device.graphicsQueue(),    std::move(commandPool),
          std::move(pipelineLayout), std::move(shaderModules),
          std::move(renderPass),     std::move(pipeline),
          std::move(framebuffers),   commandBuffers};
}

void RenderSystem::render(Frame &frame) {
  auto commandBuffer = _commandBuffers[frame.image];
  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  auto submitInfo = vk::SubmitInfo{}
                        .setWaitSemaphores(frame.readySemaphore)
                        .setWaitDstStageMask(waitStage)
                        .setCommandBuffers(commandBuffer)
                        .setSignalSemaphores(frame.doneSemaphore);
  _graphicsQueue.submit(submitInfo, frame.fence);
}
