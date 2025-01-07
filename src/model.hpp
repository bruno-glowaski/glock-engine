#pragma once

#include "buffer.hpp"

#include <ranges>

#include <vulkan/vulkan.hpp>

struct GraphicsDevice;
struct Model {
  Model(Buffer vertexBuffer, Buffer indexBuffer, uint32_t indexCount)
      : _vertexBuffer(std::move(vertexBuffer)),
        _indexBuffer(std::move(indexBuffer)), _indexCount(indexCount) {}

  template <std::ranges::range TVRange, std::ranges::range TIRange>
  static Model fromRanges(const GraphicsDevice &device,
                          vk::CommandPool workCommandPool, TVRange vertices,
                          TIRange indices);

  inline const Buffer &vertexBuffer() const { return _vertexBuffer; }
  inline const Buffer &indexBuffer() const { return _indexBuffer; }
  inline uint32_t indexCount() const { return _indexCount; }

private:
  Buffer _vertexBuffer;
  Buffer _indexBuffer;
  uint32_t _indexCount;
};
