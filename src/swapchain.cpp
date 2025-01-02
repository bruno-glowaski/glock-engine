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
  return {
      vkDevice, device.presentQueue(),  std::move(vkSwapchain), extent, *format,
      vkImages, std::move(vkImageViews)};
}

std::optional<ImageIndex> Swapchain::nextImage(vk::Semaphore readySemaphore) {
  try {
    auto result = _owner.acquireNextImageKHR(
        _vkSwapchain.get(), std::numeric_limits<uint64_t>::max(),
        readySemaphore);
    _needsRecreation = result.result == vk::Result::eSuccess;
    return result.value;
  } catch (vk::OutOfDateKHRError &) {
    _needsRecreation = true;
    return std::nullopt;
  }
}

void Swapchain::present(ImageIndex imageIndex, vk::Semaphore doneSemaphore) {
  auto presentInfo = vk::PresentInfoKHR{}
                         .setWaitSemaphores(doneSemaphore)
                         .setSwapchains(_vkSwapchain.get())
                         .setImageIndices(imageIndex);
  try {
    auto result = _presentQueue.presentKHR(presentInfo);
    _needsRecreation = result != vk::Result::eSuccess;
  } catch (vk::OutOfDateKHRError &) {
    _needsRecreation = true;
  }
}
