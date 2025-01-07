#pragma once

#include "model.hpp"

template <std::ranges::range TVRange, std::ranges::range TIRange>
Model Model::fromRanges(const GraphicsDevice &device,
                        vk::CommandPool workCommandPool, TVRange vertices,
                        TIRange indices) {
  using TIndex = std::ranges::range_value_t<TIRange>;
  static_assert(std::is_same_v<TIndex, uint16_t>,
                "Only 16-bit unsigned integers can be used as indexes.");
  uint32_t indexCount = std::ranges::size(indices);
  auto vertexBuffer =
      Buffer::createGPUOnlyArray(device, workCommandPool, vertices,
                                 vk::BufferUsageFlagBits::eVertexBuffer);
  auto indexBuffer = Buffer::createGPUOnlyArray(
      device, workCommandPool, indices, vk::BufferUsageFlagBits::eIndexBuffer);
  return {std::move(vertexBuffer), std::move(indexBuffer), indexCount};
}
