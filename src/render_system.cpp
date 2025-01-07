#include "render_system.hpp"

#include "graphics_device.hpp"
#include "material.hpp"
#include "model.hpp"
#include "swapchain.hpp"

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

vk::UniquePipeline createSimpleGraphicsPipeline(
    vk::PipelineLayout layout,
    std::span<vk::UniqueShaderModule, 2> shaderModules,
    std::span<const vk::VertexInputBindingDescription> vertexBindings,
    std::span<const vk::VertexInputAttributeDescription> vertexAttributes,
    vk::RenderPass renderPass, vk::Device device) {
  auto stages = std::to_array({
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eVertex, *shaderModules[0], "main"},
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eFragment, *shaderModules[1], "main"},
  });
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      {}, vertexBindings, vertexAttributes};
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
      {}, vk::PrimitiveTopology::eTriangleList};
  auto dynamicStates = std::to_array({
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
  });
  auto dynamicStatesInfo =
      vk::PipelineDynamicStateCreateInfo{}.setDynamicStates(dynamicStates);
  auto rasterizationState =
      vk::PipelineRasterizationStateCreateInfo{}.setLineWidth(1.0f);
  auto viewportState =
      vk::PipelineViewportStateCreateInfo{}.setViewportCount(1).setScissorCount(
          1);
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
                                  .setPViewportState(&viewportState)
                                  .setPRasterizationState(&rasterizationState)
                                  .setPMultisampleState(&multisampleState)
                                  .setPColorBlendState(&colorBlendState)
                                  .setPDynamicState(&dynamicStatesInfo)
                                  .setLayout(layout)
                                  .setRenderPass(renderPass))
                      .value;
  return pipeline;
}

RenderSystem RenderSystem::create(const GraphicsDevice &device,
                                  const Swapchain &swapchain,
                                  std::optional<RenderSystem> old) {
  auto vkDevice = device.vkDevice();

  vk::UniqueCommandPool commandPool;
  std::vector<vk::CommandBuffer> commandBuffers;
  if (old.has_value()) {
    commandPool = std::move(old->_vkCommandPool);
    commandBuffers = std::move(old->_commandBuffers);
  } else {
    commandPool = device.createGraphicsCommandPool(
        vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    commandBuffers = vkDevice.allocateCommandBuffers(
        vk::CommandBufferAllocateInfo{}
            .setCommandBufferCount(Swapchain::kMaxConcurrentFrames)
            .setCommandPool(commandPool.get()));
  }

  auto renderPass = createRenderPass(device.vkDevice(), swapchain.format());
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
  auto renderCommandPool = device.vkDevice().createCommandPoolUnique(
      vk::CommandPoolCreateInfo{{}, device.graphicsQueueIndex()});
  return {
      device.graphicsQueue(), std::move(commandPool),  commandBuffers,
      std::move(renderPass),  std::move(framebuffers),
  };
}

void RenderSystem::setMaterial(const Material &material) {
  _material = &material;
}

void RenderSystem::render(Frame &frame, vk::Extent2D extent,
                          const MeshUniforms &meshUniforms,
                          const Model &model) {
  vk::ClearValue clearValues = {
      vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
  vk::Viewport viewport{
      0.0f,
      0.0f,
      static_cast<float>(extent.width),
      static_cast<float>(extent.height),
      0.0f,
      1.0f,
  };
  vk::Rect2D scissor{{0, 0}, extent};

  auto cmd = _commandBuffers[frame.index];
  auto framebuffer = _vkFramebuffers[frame.image].get();
  cmd.reset();
  cmd.begin(vk::CommandBufferBeginInfo{});
  cmd.beginRenderPass(vk::RenderPassBeginInfo{}
                          .setRenderPass(_vkRenderPass.get())
                          .setFramebuffer(framebuffer)
                          .setClearValues(clearValues)
                          .setRenderArea(vk::Rect2D{{0, 0}, extent}),
                      vk::SubpassContents::eInline);
  cmd.setViewport(0, viewport);
  cmd.setScissor(0, scissor);
  _material->render(frame, cmd, meshUniforms, model.vertexBuffer().vkBuffer(),
                    model.indexBuffer().vkBuffer(), model.indexCount());
  cmd.setViewport(0, viewport);
  cmd.setScissor(0, scissor);
  cmd.endRenderPass();
  cmd.end();

  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  auto submitInfo = vk::SubmitInfo{}
                        .setWaitSemaphores(frame.readySemaphore)
                        .setWaitDstStageMask(waitStage)
                        .setCommandBuffers(cmd)
                        .setSignalSemaphores(frame.doneSemaphore);
  _graphicsQueue.submit(submitInfo, frame.fence);
}
