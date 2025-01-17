#pragma once

#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.hpp>

struct GraphicsDevice;
struct Buffer;
struct Texture2D {
  Texture2D(vma::UniqueImage vkImage, vma::UniqueAllocation vmaAllocation,
            vk::UniqueImageView vkImageView)
      : _vkImage(std::move(vkImage)), _vmaAllocation(std::move(vmaAllocation)),
        _vkImageView(std::move(vkImageView)) {}

  static Texture2D create(const GraphicsDevice &device, vk::Extent2D dimensions,
                          vk::Format format, vk::ImageUsageFlags usage,
                          vma::MemoryUsage memoryUsage);
  static Texture2D loadFromFile(const GraphicsDevice &device,
                                vk::CommandPool workCommandPool,
                                std::string_view filepath,
                                vk::ImageUsageFlags usage,
                                vk::ImageLayout layout);

  void copyFrom(const GraphicsDevice &device, vk::CommandPool workCommandPool,
                const Buffer &buffer, vk::Extent2D dimensions,
                vk::ImageLayout srcLayout, vk::ImageLayout dstLayout) const;

  inline vk::Image vkImage() const { return _vkImage.get(); }
  inline vk::ImageView vkImageView() const { return _vkImageView.get(); }

private:
  vma::UniqueImage _vkImage;
  vma::UniqueAllocation _vmaAllocation;
  vk::UniqueImageView _vkImageView;
};
