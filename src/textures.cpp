#include "textures.hpp"

#include <stdexcept>
#include <string>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "buffer.hpp"
#include "buffer_impl.hpp"
#include "graphics_device.hpp"
#include "graphics_device_impl.hpp"

vk::ImageAspectFlags getAspectForFormat(vk::Format format) {
  switch (format) {
  case vk::Format::eD32Sfloat:
    return vk::ImageAspectFlagBits::eDepth;
  case vk::Format::eD32SfloatS8Uint:
  case vk::Format::eD24UnormS8Uint:
    return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
  default:
    throw std::runtime_error("TBI");
  }
}

Texture2D Texture2D::create(const GraphicsDevice &device,
                            vk::Extent2D dimensions, vk::Format format,
                            vk::ImageUsageFlags usage,
                            vma::MemoryUsage memoryUsage) {
  auto [image, allocation] = device.vmaAllocator().createImageUnique(
      vk::ImageCreateInfo{}
          .setFormat(format)
          .setUsage(usage)
          .setExtent({dimensions.width, dimensions.height, 1})
          .setArrayLayers(1)
          .setMipLevels(1)
          .setImageType(vk::ImageType::e2D)
          .setTiling(vk::ImageTiling::eOptimal)
          .setSharingMode(vk::SharingMode::eExclusive)
          .setInitialLayout(vk::ImageLayout::eUndefined),
      vma::AllocationCreateInfo{}.setUsage(memoryUsage));
  auto imageView = device.vkDevice().createImageViewUnique(
      vk::ImageViewCreateInfo{}
          .setFormat(format)
          .setImage(image.get())
          .setViewType(vk::ImageViewType::e2D)
          .setSubresourceRange(vk::ImageSubresourceRange{}
                                   .setAspectMask(getAspectForFormat(format))
                                   .setLayerCount(1)
                                   .setLevelCount(1)));
  return {std::move(image), std::move(allocation), std::move(imageView)};
}

Texture2D Texture2D::loadFromFile(const GraphicsDevice &device,
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
  texture.copyFrom(device, stagingBuffer, dimensions,
                   vk::ImageLayout::eUndefined, layout);

  return texture;
}

void Texture2D::copyFrom(const GraphicsDevice &device, const Buffer &buffer,
                         vk::Extent2D dimensions, vk::ImageLayout srcLayout,
                         vk::ImageLayout dstLayout) const {
  device.runOneTimeWork([&](vk::CommandBuffer cmd) {
    cmd.pipelineBarrier(
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
    cmd.copyBufferToImage(
        buffer.vkBuffer(), vkImage(), vk::ImageLayout::eTransferDstOptimal,
        vk::BufferImageCopy{}
            .setImageExtent({dimensions.width, dimensions.height, 1})
            .setBufferRowLength(dimensions.width)
            .setImageSubresource(
                vk::ImageSubresourceLayers{}.setLayerCount(1).setAspectMask(
                    vk::ImageAspectFlagBits::eColor)));
    cmd.pipelineBarrier(
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
  });
}
