#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>
#include <vulkan/vulkan_handles.hpp>

struct GraphicsDevice;
struct Swapchain;
struct Frame;
struct RenderSystem {
  RenderSystem(vk::Queue graphicsQueue, vk::UniqueCommandPool vkCommandPool,
               std::vector<vk::CommandBuffer> commandBuffers,
               vk::UniquePipelineLayout vkPipelineLayout,
               std::array<vk::UniqueShaderModule, 2> vkShaderModules,
               vk::UniqueRenderPass vkRenderPass, vk::UniquePipeline vkPipeline,
               std::vector<vk::UniqueFramebuffer> vkFramebuffers)
      : _graphicsQueue(graphicsQueue), _vkCommandPool(std::move(vkCommandPool)),
        _commandBuffers(commandBuffers),
        _vkPipelineLayout(std::move(vkPipelineLayout)),
        _vkShaderModules(std::move(vkShaderModules)),
        _vkRenderPass(std::move(vkRenderPass)),
        _vkPipeline(std::move(vkPipeline)),
        _vkFramebuffers(std::move(vkFramebuffers)) {}

  static RenderSystem create(const GraphicsDevice &device,
                             const Swapchain &swapchain,
                             std::optional<RenderSystem> old = std::nullopt);

  void render(Frame &frame, vk::Extent2D viewport, vk::Buffer vertexBuffer,
              uint32_t vertexCount);

private:
  vk::Queue _graphicsQueue;

  // Swapchain-shared resources
  vk::UniqueCommandPool _vkCommandPool;
  std::vector<vk::CommandBuffer> _commandBuffers;
  vk::UniquePipelineLayout _vkPipelineLayout;
  std::array<vk::UniqueShaderModule, 2> _vkShaderModules;

  // Swapchain-related resources
  vk::UniqueRenderPass _vkRenderPass;
  vk::UniquePipeline _vkPipeline;

  // Swapchain-exclusive resources
  std::vector<vk::UniqueFramebuffer> _vkFramebuffers;
};
