#pragma once

#include <optional>
#include <ranges>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

using ImageIndex = uint32_t;

struct Frame {
  ImageIndex image;
  vk::Semaphore readySemaphore;
  vk::Semaphore doneSemaphore;
  vk::Fence fence;
};

struct GraphicsDevice;
struct Window;
struct Swapchain {
  const static size_t kMaxConcurrentFrames = 2;

  Swapchain(
      vk::Device owner, vk::Queue presentQueue,
      vk::UniqueSwapchainKHR vkSwapchain, vk::Extent2D extent,
      vk::SurfaceFormatKHR format, std::vector<vk::Image> vkImages,
      std::vector<vk::UniqueImageView> vkImageViews,
      std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> readySemaphores,
      std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> doneSemaphores,
      std::array<vk::UniqueFence, kMaxConcurrentFrames> frameFences,
      std::vector<vk::Fence> imageFences)
      : _owner(owner), _presentQueue(presentQueue),
        _vkSwapchain(std::move(vkSwapchain)), _extent(extent), _format(format),
        _vkImages(std::move(vkImages)), _vkImageViews(std::move(vkImageViews)),
        _readySemaphores(std::move(readySemaphores)),
        _doneSemaphores(std::move(doneSemaphores)),
        _frameFences(std::move(frameFences)), _imageFences(imageFences) {}

  static Swapchain create(const Window &window, const GraphicsDevice &device,
                          std::optional<Swapchain> oldSwapchain = std::nullopt);

  std::optional<Frame> nextImage();
  void present(Frame frame);

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

private:
  vk::Device _owner;
  vk::Queue _presentQueue;
  vk::UniqueSwapchainKHR _vkSwapchain;
  vk::Extent2D _extent;
  vk::SurfaceFormatKHR _format;
  std::vector<vk::Image> _vkImages;
  std::vector<vk::UniqueImageView> _vkImageViews;
  std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> _readySemaphores;
  std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> _doneSemaphores;
  std::array<vk::UniqueFence, kMaxConcurrentFrames> _frameFences;
  std::vector<vk::Fence> _imageFences;
  bool _needsRecreation = false;
  uint32_t _frame = 0;
};
