#pragma once

#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

struct GraphicsDevice;
struct Buffer {
  Buffer(vma::UniqueBuffer vkBuffer, vma::UniqueAllocation vmaAllocation)
      : _vkBuffer(std::move(vkBuffer)),
        _vmaAllocation(std::move(vmaAllocation)) {}

  static Buffer create(const GraphicsDevice &device, vk::BufferUsageFlags usage,
                       vma::MemoryUsage memoryUsage, size_t size);
  template <std::ranges::range TRange>
  static Buffer createGPUOnlyArray(const GraphicsDevice &device,
                                   const TRange &range,
                                   vk::BufferUsageFlags usage);
  template <class T>
  static Buffer createGPUOnly(const GraphicsDevice &device, const T &src,
                              vk::BufferUsageFlags usage);

  inline vk::Buffer vkBuffer() const { return _vkBuffer.get(); };

  template <std::ranges::range TRange>
  void copyRangeInto(const GraphicsDevice &device, const TRange &range);
  template <class T> void copyInto(const GraphicsDevice &device, const T &src);

private:
  vma::UniqueBuffer _vkBuffer;
  vma::UniqueAllocation _vmaAllocation;
};
