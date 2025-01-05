#include "swapchain.hpp"

#include <limits>
#include <vulkan/vulkan.hpp>

#include "graphics_device.hpp"
#include "window.hpp"

Swapchain Swapchain::create(const Window &window, const GraphicsDevice &device,
                            std::optional<Swapchain> oldSwapchain) {
  auto vkPhysicalDevice = device.vkPhysicalDevice();
  auto vkDevice = device.vkDevice();
  auto vkSurface = device.vkSurface();

  auto queueFamilies = device.queueFamilies();

  auto capabilities = vkPhysicalDevice.getSurfaceCapabilitiesKHR(vkSurface);
  auto formats = vkPhysicalDevice.getSurfaceFormatsKHR(vkSurface);
  auto presentModes = vkPhysicalDevice.getSurfacePresentModesKHR(vkSurface);

  auto minImageCount =
      std::max(capabilities.minImageCount + 1, capabilities.maxImageCount);
  auto format = std::ranges::find_if(formats, [](vk::SurfaceFormatKHR format) {
    return format.format == vk::Format::eB8G8R8A8Srgb &&
           format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
  });
  if (format == formats.end()) {
    format = formats.begin();
  }
  auto presentMode =
      std::ranges::contains(presentModes, vk::PresentModeKHR::eMailbox)
          ? vk::PresentModeKHR::eMailbox
          : vk::PresentModeKHR::eFifo;

  auto windowSize = window.size();
  vk::Extent2D extent{std::clamp((uint32_t)windowSize.width,
                                 capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width),
                      std::clamp((uint32_t)windowSize.height,
                                 capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};

  auto sharingMode = queueFamilies.size() == 1 ? vk::SharingMode::eExclusive
                                               : vk::SharingMode::eConcurrent;
  auto oldSwapchainHandle = oldSwapchain
                                .transform([](const Swapchain &oldSwapchain) {
                                  return oldSwapchain._vkSwapchain.get();
                                })
                                .value_or(nullptr);
  auto vkSwapchain = vkDevice.createSwapchainKHRUnique(
      vk::SwapchainCreateInfoKHR{}
          .setSurface(vkSurface)
          .setMinImageCount(minImageCount)
          .setImageFormat(format->format)
          .setImageColorSpace(format->colorSpace)
          .setImageExtent(extent)
          .setImageArrayLayers(1)
          .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
          .setImageSharingMode(sharingMode)
          .setQueueFamilyIndices(queueFamilies)
          .setPresentMode(presentMode)
          .setClipped(true)
          .setOldSwapchain(oldSwapchainHandle));
  auto vkImages = vkDevice.getSwapchainImagesKHR(*vkSwapchain);
  auto vkImageViews = vkImages |
                      std::ranges::views::transform([&](vk::Image image) {
                        return vkDevice.createImageViewUnique(
                            vk::ImageViewCreateInfo{}
                                .setImage(image)
                                .setViewType(vk::ImageViewType::e2D)
                                .setFormat(format->format)
                                .setSubresourceRange(vk::ImageSubresourceRange{
                                    vk::ImageAspectFlagBits::eColor,
                                    0,
                                    1,
                                    0,
                                    1,
                                }));
                      }) |
                      std::ranges::to<std::vector<vk::UniqueImageView>>();
  std::array<vk::UniqueFence, kMaxConcurrentFrames> frameFences;
  std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> readyImageSemaphores;
  std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> doneImageSemaphores;
  std::ranges::generate(frameFences, [&device]() {
    return device.vkDevice().createFenceUnique(
        vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
  });
  std::ranges::generate(readyImageSemaphores, [&device]() {
    return device.vkDevice().createSemaphoreUnique({});
  });
  std::ranges::generate(doneImageSemaphores, [&device]() {
    return device.vkDevice().createSemaphoreUnique({});
  });
  std::vector<vk::Fence> imageFences;
  imageFences.resize(vkImages.size());
  return {vkDevice,
          device.presentQueue(),
          std::move(vkSwapchain),
          extent,
          *format,
          vkImages,
          std::move(vkImageViews),
          std::move(readyImageSemaphores),
          std::move(doneImageSemaphores),
          std::move(frameFences),
          std::move(imageFences)};
}

std::optional<Frame> Swapchain::nextImage() {
  auto readySemaphore = _readySemaphores[_frame].get();
  auto doneSemaphore = _doneSemaphores[_frame].get();
  auto frameFence = _frameFences[_frame].get();
  _frame = (_frame + 1) % kMaxConcurrentFrames;
  std::ignore = _owner.waitForFences(frameFence, true,
                                     std::numeric_limits<uint64_t>::max());
  try {
    auto result = _owner.acquireNextImageKHR(
        _vkSwapchain.get(), std::numeric_limits<uint64_t>::max(),
        readySemaphore);
    auto image = result.value;
    auto &imageFence = _imageFences[image];
    if (imageFence != nullptr) {
      std::ignore = _owner.waitForFences(imageFence, true,
                                         std::numeric_limits<uint64_t>::max());
    }
    imageFence = frameFence;
    _owner.resetFences(frameFence);
    _needsRecreation = result.result == vk::Result::eSuccess;
    return {{_frame, image, readySemaphore, doneSemaphore, frameFence}};
  } catch (vk::OutOfDateKHRError &) {
    _needsRecreation = true;
    return std::nullopt;
  }
}

void Swapchain::present(Frame frame) {
  auto presentInfo = vk::PresentInfoKHR{}
                         .setWaitSemaphores(frame.doneSemaphore)
                         .setSwapchains(_vkSwapchain.get())
                         .setImageIndices(frame.image);
  try {
    auto result = _presentQueue.presentKHR(presentInfo);
    _needsRecreation = result != vk::Result::eSuccess;
  } catch (vk::OutOfDateKHRError &) {
    _needsRecreation = true;
  }
}
