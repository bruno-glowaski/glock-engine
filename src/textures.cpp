#include "textures.hpp"
#include "buffer.hpp"

#include <string>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "graphics_device.hpp"

Texture2D Texture2D::create(const GraphicsDevice &device,
                            vk::Extent2D dimensions, vk::Format format,
                            vk::ImageUsageFlags usage,
                            vma::MemoryUsage memoryUsage) {
  auto [image, allocation] = device.vmaAllocator().createImageUnique(
      vk::ImageCreateInfo{}
          .setUsage(usage)
          .setExtent({dimensions.width, dimensions.height, 1})
          .setTiling(vk::ImageTiling::eOptimal)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setArrayLayers(0)
          .setInitialLayout(vk::ImageLayout::eUndefined),
      vma::AllocationCreateInfo{}.setUsage(memoryUsage));
  auto imageView = device.vkDevice().createImageViewUnique(
      vk::ImageViewCreateInfo{}
          .setFormat(format)
          .setImage(image.get())
          .setViewType(vk::ImageViewType::e2D)
          .setSubresourceRange(
              vk::ImageSubresourceRange{}
                  .setAspectMask(vk::ImageAspectFlagBits::eColor)
                  .setLayerCount(1)
                  .setLevelCount(1)));
  return {std::move(image), std::move(allocation), std::move(imageView)};
}

Texture2D Texture2D::loadFromFile(const GraphicsDevice &device,
                                  vk::CommandPool workCommandPool,
                                  std::string_view filepath,
                                  vk::ImageUsageFlags usage,
                                  vk::ImageLayout layout) {
  std::string filepathOwned(filepath);
  const int channels = 4;
  int width, height;
  auto imageData =
      stbi_load(filepathOwned.c_str(), &width, &height, nullptr, channels);
  size_t size = static_cast<size_t>(width * height * channels);
  auto stagingBuffer =
      Buffer::create(device, vk::BufferUsageFlagBits::eTransferSrc,
                     vma::MemoryUsage::eCpuToGpu, size);
  stagingBuffer.copyRangeInto(device, std::span{imageData, size});
  stbi_image_free(imageData);

  vk::Extent2D dimensions{static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height)};
  auto texture =
      Texture2D::create(device, dimensions, vk::Format::eR8G8B8A8Srgb, usage,
                        vma::MemoryUsage::eGpuOnly);
  texture.copyFrom(device, workCommandPool, stagingBuffer, dimensions,
                   vk::ImageLayout::eUndefined, layout);

  return texture;
}

void Texture2D::copyFrom(const GraphicsDevice &device,
                         vk::CommandPool workCommandPool, const Buffer &buffer,
                         vk::Extent2D dimensions, vk::ImageLayout srcLayout,
                         vk::ImageLayout dstLayout) const {
  auto vkDevice = device.vkDevice();
  auto queue = device.workQueue();
  auto commandBuffer =
      vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo{}
                                          .setCommandPool(workCommandPool)
                                          .setCommandBufferCount(1))[0];
  commandBuffer.begin(vk::CommandBufferBeginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  commandBuffer.pipelineBarrier(
      vk::PipelineStageFlagBits::eTopOfPipe,
      vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
      vk::ImageMemoryBarrier{}
          .setImage(vkImage())
          .setSubresourceRange(
              vk::ImageSubresourceRange{}
                  .setAspectMask(vk::ImageAspectFlagBits::eColor)
                  .setLayerCount(1)
                  .setLevelCount(1))
          .setOldLayout(srcLayout)
          .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
          .setDstAccessMask(vk::AccessFlagBits::eTransferWrite));
  commandBuffer.copyBufferToImage(
      buffer.vkBuffer(), vkImage(), vk::ImageLayout::eTransferDstOptimal,
      vk::BufferImageCopy{}
          .setImageExtent({dimensions.width, dimensions.height, 1})
          .setBufferRowLength(dimensions.width)
          .setImageSubresource(
              vk::ImageSubresourceLayers{}.setLayerCount(1).setAspectMask(
                  vk::ImageAspectFlagBits::eColor)));
  commandBuffer.pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {},
      vk::ImageMemoryBarrier{}
          .setImage(vkImage())
          .setSubresourceRange(
              vk::ImageSubresourceRange{}
                  .setAspectMask(vk::ImageAspectFlagBits::eColor)
                  .setLayerCount(1)
                  .setLevelCount(1))
          .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
          .setNewLayout(dstLayout)
          .setDstAccessMask(vk::AccessFlagBits::eTransferWrite));
  commandBuffer.end();
  queue.submit(vk::SubmitInfo{}.setCommandBuffers(commandBuffer));
  queue.waitIdle();
  vkDevice.freeCommandBuffers(workCommandPool, commandBuffer);
}
