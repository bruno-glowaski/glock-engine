#include "buffer.hpp"

#include "graphics_device.hpp"
#include <ranges>

template <std::ranges::range TRange>
Buffer Buffer::createGPUOnlyArray(const GraphicsDevice &device,
                                  vk::CommandPool commandPool,
                                  const TRange &range,
                                  vk::BufferUsageFlags usage) {
  using TItem = std::ranges::range_value_t<TRange>;

  auto vkDevice = device.vkDevice();
  auto queue = device.workQueue();

  const size_t size = sizeof(TItem) * std::ranges::size(range);

  auto finalBuffer =
      Buffer::create(device, usage | vk::BufferUsageFlagBits::eTransferDst,
                     vma::MemoryUsage::eGpuOnly, size);
  auto stagingBuffer =
      Buffer::create(device, vk::BufferUsageFlagBits::eTransferSrc,
                     vma::MemoryUsage::eCpuToGpu, size);
  stagingBuffer.copyRangeInto(device, range);

  auto commandBuffer =
      vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo{}
                                          .setCommandPool(commandPool)
                                          .setCommandBufferCount(1))[0];
  commandBuffer.begin(vk::CommandBufferBeginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  commandBuffer.copyBuffer(stagingBuffer.vkBuffer(), finalBuffer.vkBuffer(),
                           vk::BufferCopy{}.setSize(size));
  commandBuffer.end();
  queue.submit(vk::SubmitInfo{}.setCommandBuffers(commandBuffer));
  queue.waitIdle();
  vkDevice.freeCommandBuffers(commandPool, commandBuffer);

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
