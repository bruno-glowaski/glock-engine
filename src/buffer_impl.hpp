#include "buffer.hpp"

#include <cstring>
#include <ranges>

#include "graphics_device.hpp"
#include "graphics_device_impl.hpp"

template <std::ranges::range TRange>
Buffer Buffer::createGPUOnlyArray(const GraphicsDevice &device,
                                  const TRange &range,
                                  vk::BufferUsageFlags usage) {
  using TItem = std::ranges::range_value_t<TRange>;

  const size_t size = sizeof(TItem) * std::ranges::size(range);

  auto finalBuffer =
      Buffer::create(device, usage | vk::BufferUsageFlagBits::eTransferDst,
                     vma::MemoryUsage::eGpuOnly, size);
  auto stagingBuffer =
      Buffer::create(device, vk::BufferUsageFlagBits::eTransferSrc,
                     vma::MemoryUsage::eCpuToGpu, size);
  stagingBuffer.copyRangeInto(device, range);

  device.runOneTimeWork([&](vk::CommandBuffer cmd) {
    cmd.copyBuffer(stagingBuffer.vkBuffer(), finalBuffer.vkBuffer(),
                   vk::BufferCopy{}.setSize(size));
  });

  return finalBuffer;
}

template <class T>
Buffer Buffer::createGPUOnly(const GraphicsDevice &device, const T &src,
                             vk::BufferUsageFlags usage) {
  const size_t size = sizeof(T);

  auto finalBuffer =
      Buffer::create(device, usage | vk::BufferUsageFlagBits::eTransferDst,
                     vma::MemoryUsage::eGpuOnly, size);
  auto stagingBuffer =
      Buffer::create(device, vk::BufferUsageFlagBits::eTransferSrc,
                     vma::MemoryUsage::eCpuToGpu, size);
  stagingBuffer.copyInto(device, src);

  device.runOneTimeWork([&](vk::CommandBuffer cmd) {
    cmd.copyBuffer(stagingBuffer.vkBuffer(), finalBuffer.vkBuffer(),
                   vk::BufferCopy{}.setSize(size));
  });

  return finalBuffer;
}

template <std::ranges::range TRange>
void Buffer::copyRangeInto(const GraphicsDevice &device, const TRange &src) {
  using TItem = std::ranges::range_value_t<TRange>;
  TItem *dest = reinterpret_cast<TItem *>(
      device.vmaAllocator().mapMemory(_vmaAllocation.get()));
  std::ranges::copy(src, dest);
  device.vmaAllocator().unmapMemory(_vmaAllocation.get());
}

template <class T>
void Buffer::copyInto(const GraphicsDevice &device, const T &src) {
  void *dest = device.vmaAllocator().mapMemory(_vmaAllocation.get());
  std::memcpy(dest, &src, sizeof(T));
  device.vmaAllocator().unmapMemory(_vmaAllocation.get());
}
