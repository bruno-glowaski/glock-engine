#pragma once

#include <chrono>
#include <optional>
#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

#include "../buffer.hpp"
#include "../material.hpp"

struct GraphicsDevice;
struct RenderSystem;
struct ColorfulMaterial : public Material {
  using Duration = std::chrono::duration<float>;

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
    float time;
  };

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

  template <class TDuration> inline void setTime(const TDuration &value) {
    _time = std::chrono::duration_cast<Duration>(value).count();
  }
  void render(const Frame &frame, vk::CommandBuffer cmd,
              const MeshUniforms &meshUniforms, vk::Buffer vertexBuffer,
              vk::Buffer indexBuffer, uint32_t indexCount) const;

private:
  vk::UniquePipelineLayout _vkPipelineLayout;
  std::array<vk::UniqueShaderModule, 2> _vkShaderModules;
  vk::UniquePipeline _vkPipeline;
  float _time = 0.0;
};
