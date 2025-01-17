#pragma once

#include "graphics_device.hpp"

template <std::invocable<vk::CommandBuffer> TCommandBuilder>
void GraphicsDevice::runOneTimeWork(TCommandBuilder buildFn) const {
  auto commandBuffer = _vkDevice->allocateCommandBuffers(
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(_workCommandPool.get())
          .setCommandBufferCount(1))[0];
  commandBuffer.begin(vk::CommandBufferBeginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  buildFn(commandBuffer);
  commandBuffer.end();
  _workQueue.submit(vk::SubmitInfo{}.setCommandBuffers(commandBuffer));
  _workQueue.waitIdle();
  _vkDevice->freeCommandBuffers(_workCommandPool.get(), commandBuffer);
}
