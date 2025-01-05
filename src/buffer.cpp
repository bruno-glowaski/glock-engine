#include "buffer_impl.hpp"

#include "graphics_device.hpp"

Buffer Buffer::create(const GraphicsDevice &device, vk::BufferUsageFlags usage,
                      vma::MemoryUsage memoryUsage, size_t size) {
  auto raw = device.vmaAllocator().createBufferUnique(
      vk::BufferCreateInfo{}
          .setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
          .setSize(size),
      vma::AllocationCreateInfo{}.setUsage(memoryUsage));
  return {std::move(raw.first), std::move(raw.second)};
}
