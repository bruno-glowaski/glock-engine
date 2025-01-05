#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include "../buffer.hpp"
#include "../material.hpp"

struct GraphicsDevice;
struct RenderSystem;
struct ColorfulMaterial : public Material {
  ColorfulMaterial(vk::UniquePipelineLayout vkPipelineLayout,
                   std::array<vk::UniqueShaderModule, 2> vkShaderModules,
                   vk::UniquePipeline vkPipeline)
      : _vkPipelineLayout(std::move(vkPipelineLayout)),
        _vkShaderModules(std::move(vkShaderModules)),
        _vkPipeline(std::move(vkPipeline)) {}

  static ColorfulMaterial
  create(const GraphicsDevice &device, const RenderSystem &renderSystem,
         std::optional<ColorfulMaterial> old = std::nullopt);

  struct PerFrameUniforms {
    float ellapsedTime;
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

  inline void setEllapsedTime(float value) { _ellapsedTime = value; }
  void render(const Frame &frame, vk::CommandBuffer cmd,
              vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
              uint32_t indexCount) const;

private:
  vk::UniquePipelineLayout _vkPipelineLayout;
  std::array<vk::UniqueShaderModule, 2> _vkShaderModules;
  vk::UniquePipeline _vkPipeline;
  float _ellapsedTime = 0.0;
};
