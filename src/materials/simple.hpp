#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include "../buffer.hpp"
#include "../material.hpp"

struct GraphicsDevice;
struct RenderSystem;
struct SimpleMaterial : public Material {
  SimpleMaterial(vk::UniqueDescriptorPool vkDescriptorPool,
                 vk::UniqueDescriptorSetLayout vkSetLayout,
                 vk::UniquePipelineLayout vkPipelineLayout,
                 Buffer perMaterialUBO,
                 vk::DescriptorSet perMaterialDescriptorSet,
                 std::array<vk::UniqueShaderModule, 2> vkShaderModules,
                 vk::UniquePipeline vkPipeline)
      : _vkDescriptorPool(std::move(vkDescriptorPool)),
        _vkSetLayout(std::move(vkSetLayout)),
        _vkPipelineLayout(std::move(vkPipelineLayout)),
        _perMaterialUBO(std::move(perMaterialUBO)),
        _perMaterialDescriptorSet(perMaterialDescriptorSet),
        _vkShaderModules(std::move(vkShaderModules)),
        _vkPipeline(std::move(vkPipeline)) {}

  static SimpleMaterial
  create(const GraphicsDevice &device, vk::CommandPool workCommandPool,
         const RenderSystem &renderSystem, glm::vec3 color,
         std::optional<SimpleMaterial> old = std::nullopt);

  struct PerMaterialUniforms {
    glm::vec3 color;
  };

  struct Vertex {
    glm::vec3 position;

    static constexpr auto kBindings = std::to_array({
        vk::VertexInputBindingDescription{0, sizeof(glm::vec3)},
    });
    static constexpr auto kAttributes = std::to_array({
        vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat,
                                            0},
    });
  };

  void render(const Frame &frame, vk::CommandBuffer cmd,
              vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
              uint32_t indexCount) const;

private:
  vk::UniqueDescriptorPool _vkDescriptorPool;
  vk::UniqueDescriptorSetLayout _vkSetLayout;
  vk::UniquePipelineLayout _vkPipelineLayout;
  Buffer _perMaterialUBO;
  vk::DescriptorSet _perMaterialDescriptorSet;
  std::array<vk::UniqueShaderModule, 2> _vkShaderModules;
  vk::UniquePipeline _vkPipeline;
};
