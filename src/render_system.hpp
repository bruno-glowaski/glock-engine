#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <vulkan/vulkan_handles.hpp>

struct GraphicsDevice;
struct Swapchain;
struct Frame;
struct RenderSystem {
  RenderSystem(vk::Queue graphicsQueue, vk::UniqueCommandPool vkCommandPool,
               vk::UniquePipelineLayout vkPipelineLayout,
               std::array<vk::UniqueShaderModule, 2> vkShaderModules,
               vk::UniqueRenderPass vkRenderPass, vk::UniquePipeline vkPipeline,
               std::vector<vk::UniqueFramebuffer> vkFramebuffers,
               std::vector<vk::CommandBuffer> commandBuffers)
      : _graphicsQueue(graphicsQueue), _vkCommandPool(std::move(vkCommandPool)),
        _vkPipelineLayout(std::move(vkPipelineLayout)),
        _vkShaderModules(std::move(vkShaderModules)),
        _vkRenderPass(std::move(vkRenderPass)),
        _vkPipeline(std::move(vkPipeline)),
        _vkFramebuffers(std::move(vkFramebuffers)),
        _commandBuffers(commandBuffers) {}

  static RenderSystem create(const GraphicsDevice &device,
                             const Swapchain &swapchain,
                             vk::Buffer vertexBuffer, uint32_t vertexCount,
                             std::optional<RenderSystem> old = std::nullopt);

  void render(Frame &frame);

private:
  vk::Queue _graphicsQueue;

  // Swapchain-shared resources
  vk::UniqueCommandPool _vkCommandPool;
  vk::UniquePipelineLayout _vkPipelineLayout;
  std::array<vk::UniqueShaderModule, 2> _vkShaderModules;

  // Swapchain-related resources
  vk::UniqueRenderPass _vkRenderPass;
  vk::UniquePipeline _vkPipeline;

  // Swapchain-exclusive resources
  std::vector<vk::UniqueFramebuffer> _vkFramebuffers;
  // TODO: re-record command buffers
  std::vector<vk::CommandBuffer> _commandBuffers;
};
