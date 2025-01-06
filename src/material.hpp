#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

struct MeshUniforms {
  glm::mat4 mvp;
};

struct Frame;
struct Material {
  virtual ~Material() {}
  virtual void render(const Frame &frame, vk::CommandBuffer cmd,
                      const MeshUniforms &meshUniforms, vk::Buffer vertexBuffer,
                      vk::Buffer indexBuffer, uint32_t indexCount) const = 0;
};
