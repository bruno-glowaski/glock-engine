#include "colorful.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "../buffer_impl.hpp"
#include "../graphics_device.hpp"
#include "../render_system.hpp"
#include "../swapchain.hpp"
#include "utils.hpp"

const auto kVertShaderPath = "./assets/shaders/colorful.vert.spv";
const auto kFragShaderPath = "./assets/shaders/colorful.frag.spv";

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
      ColorfulMaterial::Vertex::kBindings,
      ColorfulMaterial::Vertex::kAttributes};
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

ColorfulMaterial ColorfulMaterial::create(const GraphicsDevice &device,
                                          const RenderSystem &renderSystem,
                                          std::optional<ColorfulMaterial>) {
  auto vkDevice = device.vkDevice();

  auto pushConstantRanges = std::to_array({
      vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, 0,
                            sizeof(PerFrameUniforms)},
  });
  static_assert(sizeof(PerFrameUniforms) < 128,
                "Push constant uniforms bigger than minimum guaranteed size.");
  auto pipelineLayout = vkDevice.createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo{}.setPushConstantRanges(pushConstantRanges));

  auto shaderModules = std::to_array({
      loadShaderFromPath(kVertShaderPath, vkDevice),
      loadShaderFromPath(kFragShaderPath, vkDevice),
  });
  auto pipeline = createPipeline(pipelineLayout.get(), shaderModules,
                                 renderSystem.vkRenderPass(), vkDevice);

  return {std::move(pipelineLayout), std::move(shaderModules),
          std::move(pipeline)};
}

void ColorfulMaterial::render(const Frame &, vk::CommandBuffer cmd,
                              vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
                              uint32_t indexCount) const {
  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _vkPipeline.get());
  cmd.pushConstants<PerFrameUniforms>(_vkPipelineLayout.get(),
                                      vk::ShaderStageFlagBits::eFragment, 0,
                                      PerFrameUniforms{_time});
  cmd.bindVertexBuffers(0, vertexBuffer, {0});
  cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);
  cmd.drawIndexed(indexCount, 1, 0, 0, 0);
}
