#pragma once

#include <vulkan/vulkan.hpp>

#include <glm/glm.hpp>

struct Frame;
struct Material {
  virtual ~Material() {}
  virtual void render(const Frame &frame, vk::CommandBuffer cmd,
                      vk::Buffer vertexBuffer, vk::Buffer indexBuffer,
                      uint32_t indexCount) const = 0;
};
