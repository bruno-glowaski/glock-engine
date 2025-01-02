#pragma once

#include <optional>
#include <ranges>

#include <vulkan/vulkan.hpp>

using ImageIndex = uint32_t;

struct GraphicsDevice;
struct Window;
struct Swapchain {
  Swapchain(vk::Device owner, vk::Queue presentQueue,
            vk::UniqueSwapchainKHR vkSwapchain, vk::Extent2D extent,
            vk::SurfaceFormatKHR format, std::vector<vk::Image> vkImages,
            std::vector<vk::UniqueImageView> vkImageViews)
      : _owner(owner), _presentQueue(presentQueue),
        _vkSwapchain(std::move(vkSwapchain)), _extent(extent), _format(format),
        _vkImages(std::move(vkImages)), _vkImageViews(std::move(vkImageViews)) {
  }

  static Swapchain create(const Window &window, const GraphicsDevice &device,
                          std::optional<Swapchain> oldSwapchain = std::nullopt);

  std::optional<ImageIndex> nextImage(vk::Semaphore readySemaphore);
  void present(ImageIndex imageIndex, vk::Semaphore doneSemaphore);

  inline vk::Extent2D extent() const { return _extent; };
  inline vk::Format format() const { return _format.format; };
  inline vk::ColorSpaceKHR colorSpace() const { return _format.colorSpace; };
  inline size_t imageCount() const { return _vkImages.size(); }
  inline auto images() const {
    return _vkImageViews |
           std::ranges::views::transform(
               [](const vk::UniqueImageView &s) { return s.get(); });
  }
  inline bool needsRecreation() const { return _needsRecreation; }

public:
  vk::Device _owner;
  vk::Queue _presentQueue;
  vk::UniqueSwapchainKHR _vkSwapchain;
  vk::Extent2D _extent;
  vk::SurfaceFormatKHR _format;
  std::vector<vk::Image> _vkImages;
  std::vector<vk::UniqueImageView> _vkImageViews;
  bool _needsRecreation = false;
};
