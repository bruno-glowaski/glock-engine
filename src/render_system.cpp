#include "render_system.hpp"

#include <array>
#include <ranges>

#include "graphics_device.hpp"
#include "material.hpp"
#include "model.hpp"
#include "swapchain.hpp"

vk::UniqueRenderPass createRenderPass(vk::Device device,
                                      vk::Format swapchainFormat,
                                      vk::Format depthFormat) {
  auto attachments = std::to_array({
      vk::AttachmentDescription{}
          .setFormat(swapchainFormat)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
      vk::AttachmentDescription{}
          .setFormat(depthFormat)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
  });
  auto attachmentsRefs = std::to_array({
      vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal},
      vk::AttachmentReference{1,
                              vk::ImageLayout::eDepthStencilAttachmentOptimal},
  });
  auto subpasses = std::to_array({
      vk::SubpassDescription{}
          .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
          .setColorAttachments(attachmentsRefs[0])
          .setPDepthStencilAttachment(&attachmentsRefs[1]),
  });
  auto subpassDependencies = std::to_array({
      vk::SubpassDependency{}
          .setSrcSubpass(vk::SubpassExternal)
          .setDstSubpass(0)
          .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests)
          .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                           vk::PipelineStageFlagBits::eEarlyFragmentTests)
          .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite |
                            vk::AccessFlagBits::eDepthStencilAttachmentWrite),
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

  auto renderPass = createRenderPass(device.vkDevice(), swapchain.format(),
                                     device.depthFormat());
  auto depthBuffers =
      std::views::iota(static_cast<size_t>(0), swapchain.imageCount()) |
      std::views::transform([&](auto) {
        return Texture2D::create(
            device, swapchain.extent(), device.depthFormat(),
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vma::MemoryUsage::eGpuOnly);
      }) |
      std::ranges::to<std::vector<Texture2D>>();
  auto framebuffers = std::views::zip_transform(
                          [&](vk::ImageView color, const Texture2D &depth) {
                            auto attachments = {color, depth.vkImageView()};
                            return device.vkDevice().createFramebufferUnique(
                                vk::FramebufferCreateInfo{}
                                    .setRenderPass(*renderPass)
                                    .setAttachments(attachments)
                                    .setWidth(swapchain.extent().width)
                                    .setHeight(swapchain.extent().height)
                                    .setLayers(1));
                          },
                          swapchain.images(), depthBuffers) |
                      std::ranges::to<std::vector>();
  return {
      device.graphicsQueue(), std::move(commandPool),  commandBuffers,
      std::move(renderPass),  std::move(depthBuffers), std::move(framebuffers),
  };
}

void RenderSystem::render(
    Frame &frame, vk::Extent2D extent,
    std::initializer_list<
        std::tuple<Material &, const MeshUniforms &, const Model &>>
        objects) {
  auto clearValues = std::to_array<vk::ClearValue>(
      {vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}},
       vk::ClearDepthStencilValue{1.0, 0}});
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
  for (auto [material, uniforms, model] : objects) {
    material.render(frame, cmd, uniforms, model.vertexBuffer().vkBuffer(),
                    model.indexBuffer().vkBuffer(), model.indexCount());
  }
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
