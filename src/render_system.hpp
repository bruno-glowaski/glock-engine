#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

struct GraphicsDevice;
struct Swapchain;
struct Material;
struct Frame;
struct MeshUniforms;
struct Model;
struct RenderSystem {
  RenderSystem(vk::Queue graphicsQueue, vk::UniqueCommandPool vkCommandPool,
               std::vector<vk::CommandBuffer> commandBuffers,
               vk::UniqueRenderPass vkRenderPass,
               std::vector<vk::UniqueFramebuffer> vkFramebuffers)
      : _graphicsQueue(graphicsQueue), _vkCommandPool(std::move(vkCommandPool)),
        _commandBuffers(commandBuffers), _vkRenderPass(std::move(vkRenderPass)),
        _vkFramebuffers(std::move(vkFramebuffers)) {}

  static RenderSystem create(const GraphicsDevice &device,
                             const Swapchain &swapchain,
                             std::optional<RenderSystem> old = std::nullopt);

  inline vk::RenderPass vkRenderPass() const { return _vkRenderPass.get(); }

  void setMaterial(const Material &material);
  void render(Frame &frame, vk::Extent2D viewport,
              const MeshUniforms &meshUniforms, const Model &model);

private:
  vk::Queue _graphicsQueue;

  // Swapchain-shared resources
  vk::UniqueCommandPool _vkCommandPool;
  std::vector<vk::CommandBuffer> _commandBuffers;

  // Swapchain-related resources
  const Material *_material = nullptr;
  vk::UniqueRenderPass _vkRenderPass;

  // Swapchain-exclusive resources
  std::vector<vk::UniqueFramebuffer> _vkFramebuffers;
};
