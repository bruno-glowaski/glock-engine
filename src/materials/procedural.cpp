#include "procedural.hpp"

#include <vulkan/vulkan.hpp>

#include "../graphics_device.hpp"
#include "../render_system.hpp"
#include "../swapchain.hpp"
#include "utils.hpp"

static vk::UniquePipeline
createPipeline(vk::PipelineLayout layout,
               std::span<vk::UniqueShaderModule, 2> shaderModules,
               vk::RenderPass renderPass, vk::Device device) {
  auto stages = std::to_array({
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eVertex, *shaderModules[0], "main"},
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eFragment, *shaderModules[1], "main"},
  });
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      {},
      ProceduralMaterial::Vertex::kBindings,
      ProceduralMaterial::Vertex::kAttributes};
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
  auto depthStencilState = vk::PipelineDepthStencilStateCreateInfo{}
                               .setDepthTestEnable(true)
                               .setDepthWriteEnable(true)
                               .setDepthCompareOp(vk::CompareOp::eLess);
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
                                  .setPDepthStencilState(&depthStencilState)
                                  .setPColorBlendState(&colorBlendState)
                                  .setPDynamicState(&dynamicStatesInfo)
                                  .setLayout(layout)
                                  .setRenderPass(renderPass))
                      .value;
  return pipeline;
}

ProceduralMaterial ProceduralMaterial::create(
    const GraphicsDevice &device, const RenderSystem &renderSystem,
    std::string_view vertShaderPath, std::string_view fragShaderPath,
    std::optional<ProceduralMaterial>) {
  auto vkDevice = device.vkDevice();

  auto pushConstantRanges = std::to_array({vk::PushConstantRange{
      kVertexAndFragmentStages, 0, sizeof(MeshUniforms)}});
  static_assert(sizeof(MeshUniforms) < 128,
                "Push constant bigger than minimum guaranteed size.");
  auto pipelineLayout = vkDevice.createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo{}.setPushConstantRanges(pushConstantRanges));

  auto shaderModules = std::to_array({
      loadShaderFromPath(vertShaderPath, vkDevice),
      loadShaderFromPath(fragShaderPath, vkDevice),
  });
  auto pipeline = createPipeline(pipelineLayout.get(), shaderModules,
                                 renderSystem.vkRenderPass(), vkDevice);

  return {std::move(pipelineLayout), std::move(shaderModules),
          std::move(pipeline)};
}

void ProceduralMaterial::render(const Frame &, vk::CommandBuffer cmd,
                                const MeshUniforms &meshUniforms,
                                vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
                                uint32_t indexCount) const {
  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _vkPipeline.get());
  cmd.pushConstants<MeshUniforms>(_vkPipelineLayout.get(),
                                  kVertexAndFragmentStages, 0, meshUniforms);
  cmd.bindVertexBuffers(0, vertexBuffer, {0});
  cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);
  cmd.drawIndexed(indexCount, 1, 0, 0, 0);
}
