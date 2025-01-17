#include "simple.hpp"

#include <fstream>
#include <ios>
#include <string_view>

#include <vulkan/vulkan.hpp>

#include "../buffer_impl.hpp"
#include "../graphics_device.hpp"
#include "../render_system.hpp"
#include "../swapchain.hpp"
#include "utils.hpp"

const auto kVertShaderPath = "./assets/shaders/simple.vert.spv";
const auto kFragShaderPath = "./assets/shaders/simple.frag.spv";

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
      SimpleMaterial::Vertex::kBindings,
      SimpleMaterial::Vertex::kAttributes};
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

SimpleMaterial SimpleMaterial::create(const GraphicsDevice &device,
                                      const RenderSystem &renderSystem,
                                      glm::vec3 color,
                                      std::optional<SimpleMaterial>) {
  auto vkDevice = device.vkDevice();

  auto poolSizes = std::to_array<vk::DescriptorPoolSize>({
      {vk::DescriptorType::eUniformBuffer, 1},
  });
  auto descriptorPool = vkDevice.createDescriptorPoolUnique(
      vk::DescriptorPoolCreateInfo{}.setMaxSets(1).setPoolSizes(poolSizes));

  auto setBindings = std::to_array<vk::DescriptorSetLayoutBinding>({
      {0, vk::DescriptorType::eUniformBuffer, 1,
       vk::ShaderStageFlagBits::eFragment},
  });
  auto setLayout = vkDevice.createDescriptorSetLayoutUnique(
      vk::DescriptorSetLayoutCreateInfo{}.setBindings(setBindings));
  auto pushConstantRanges = std::to_array({vk::PushConstantRange{
      kVertexAndFragmentStages, 0, sizeof(MeshUniforms)}});
  static_assert(sizeof(MeshUniforms) < 128,
                "Push constant bigger than minimum guaranteed size.");
  auto pipelineLayout = vkDevice.createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo{}
          .setSetLayouts(setLayout.get())
          .setPushConstantRanges(pushConstantRanges));

  auto perMaterialUBO =
      Buffer::createGPUOnly(device, PerMaterialUniforms{color},
                            vk::BufferUsageFlagBits::eUniformBuffer);
  auto perMaterialDescriptorSet = vkDevice.allocateDescriptorSets(
      vk::DescriptorSetAllocateInfo{}
          .setSetLayouts(setLayout.get())
          .setDescriptorPool(descriptorPool.get()))[0];
  auto perMaterialUBOInfo = vk::DescriptorBufferInfo{}
                                .setBuffer(perMaterialUBO.vkBuffer())
                                .setRange(vk::WholeSize);
  auto descriptorWrites = std::to_array({
      vk::WriteDescriptorSet{}
          .setDstSet(perMaterialDescriptorSet)
          .setDstBinding(0)
          .setDescriptorCount(1)
          .setDescriptorType(vk::DescriptorType::eUniformBuffer)
          .setBufferInfo(perMaterialUBOInfo),
  });
  vkDevice.updateDescriptorSets(descriptorWrites, {});

  auto shaderModules = std::to_array({
      loadShaderFromPath(kVertShaderPath, vkDevice),
      loadShaderFromPath(kFragShaderPath, vkDevice),
  });
  auto pipeline = createPipeline(pipelineLayout.get(), shaderModules,
                                 renderSystem.vkRenderPass(), vkDevice);

  return {std::move(descriptorPool), std::move(setLayout),
          std::move(pipelineLayout), std::move(perMaterialUBO),
          perMaterialDescriptorSet,  std::move(shaderModules),
          std::move(pipeline)};
}

void SimpleMaterial::render(const Frame &, vk::CommandBuffer cmd,
                            const MeshUniforms &meshUniforms,
                            vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
                            uint32_t indexCount) const {
  cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _vkPipeline.get());
  cmd.pushConstants<MeshUniforms>(_vkPipelineLayout.get(),
                                  kVertexAndFragmentStages, 0, meshUniforms);
  cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                         _vkPipelineLayout.get(), 0, _perMaterialDescriptorSet,
                         {});
  cmd.bindVertexBuffers(0, vertexBuffer, {0});
  cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);
  cmd.drawIndexed(indexCount, 1, 0, 0, 0);
}
