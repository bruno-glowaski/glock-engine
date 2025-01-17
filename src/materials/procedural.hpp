#pragma once

#include <string_view>

#include <glm/glm.hpp>

#include <vulkan/vulkan.hpp>

#include "../buffer.hpp"
#include "../material.hpp"

struct GraphicsDevice;
struct RenderSystem;
struct ProceduralMaterial : public Material {
  ProceduralMaterial(vk::UniquePipelineLayout vkPipelineLayout,
                     std::array<vk::UniqueShaderModule, 2> vkShaderModules,
                     vk::UniquePipeline vkPipeline)
      : _vkPipelineLayout(std::move(vkPipelineLayout)),
        _vkShaderModules(std::move(vkShaderModules)),
        _vkPipeline(std::move(vkPipeline)) {}

  static ProceduralMaterial
  create(const GraphicsDevice &device, const RenderSystem &renderSystem,
         std::string_view vertShaderPath, std::string_view fragShaderPath,
         std::optional<ProceduralMaterial> old = std::nullopt);

  struct Vertex {
    Vertex(float x, float y, float z) : position(x, y, z) {}

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
              const MeshUniforms &meshUniforms, vk::Buffer vertexBuffer,
              vk::Buffer indexBuffer, uint32_t indexCount) const;

private:
  vk::UniquePipelineLayout _vkPipelineLayout;
  std::array<vk::UniqueShaderModule, 2> _vkShaderModules;
  vk::UniquePipeline _vkPipeline;
};
