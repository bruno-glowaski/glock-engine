#define VMA_IMPLEMENTATION
#include "graphics_device.hpp"

#include <print>
#include <ranges>
#include <string>

#include <vulkan/vulkan.hpp>

namespace views = std::ranges::views;

#include "window.hpp"

const auto kEngineName = "Glock Engine";
const auto kEngineVersion = VK_MAKE_VERSION(0, 1, 0);

const auto kVkLayers = std::to_array({"VK_LAYER_KHRONOS_validation"});
const auto kVkDeviceExtensions = std::to_array({vk::KHRSwapchainExtensionName});

vk::UniqueInstance createInstance(std::string_view appName,
                                  AppVersion appVersion) {
  std::string appNameOwned = std::string(appName);
  vk::ApplicationInfo kApplicationInfo = {
      appNameOwned.c_str(), appVersion,       kEngineName,
      kEngineVersion,       vk::ApiVersion13,
  };
  uint32_t vkInstanceExtensionCount;
  const char **vkInstanceExtensions =
      glfwGetRequiredInstanceExtensions(&vkInstanceExtensionCount);
  return vk::createInstanceUnique(vk::InstanceCreateInfo{
      {},
      &kApplicationInfo,
      kVkLayers.size(),
      kVkLayers.data(),
      vkInstanceExtensionCount,
      vkInstanceExtensions,
  });
}

vk::UniqueSurfaceKHR createSurface(const vk::Instance instance,
                                   GLFWwindow *window) {
  VkSurfaceKHR rawSurface;
  VkResult rawResult =
      glfwCreateWindowSurface(instance, window, nullptr, &rawSurface);
  if (rawResult != static_cast<VkResult>(vk::Result::eSuccess)) {
    throw std::runtime_error(
        std::format("failed to create window. Result code: {}",
                    static_cast<int>(rawResult)));
  }
  vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic> deleter(instance);
  return vk::UniqueSurfaceKHR{rawSurface, deleter};
}

void logPhysicalDevicesInfo(std::span<vk::PhysicalDevice> physicalDevices) {
  for (auto [i, physicalDevice] : physicalDevices | views::enumerate) {
    auto properties = physicalDevice.getProperties();
    auto extensions = physicalDevice.enumerateDeviceExtensionProperties();
    auto allQueueFamilies = physicalDevice.getQueueFamilyProperties();
    std::println("{}. {} [{}:{}]", i, properties.deviceName.data(),
                 properties.vendorID, properties.deviceID);
    std::println("- Extensions:");
    for (auto extension : extensions) {
      std::println("  - {}", extension.extensionName.data());
    }
  }
}

std::tuple<vk::PhysicalDevice, std::array<QueueIndex, 2>, uint32_t>
autoselectPhysicalDevice(std::span<vk::PhysicalDevice> physicalDevices,
                         const vk::SurfaceKHR surface) {
  std::optional<vk::PhysicalDevice> selectedDevice;
  std::array<uint32_t, 2> queueFamilies;
  uint32_t queueFamilyCount;
  for (auto [i, device] : physicalDevices | views::enumerate) {
    // 1. Needs a Graphics queue and a Present queue;
    // 2. Needs to support all required device extensions
    auto extensions = device.enumerateDeviceExtensionProperties();
    auto allQueueFamilies = device.getQueueFamilyProperties();

    std::optional<uint32_t> graphicsQueue, presentQueue;
    for (auto [i, queueFamily] : allQueueFamilies | views::enumerate) {
      auto index = (uint32_t)i;
      if (!graphicsQueue.has_value() &&
          queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
        graphicsQueue = index;
      }

      if ((!presentQueue.has_value() || presentQueue == graphicsQueue) &&
          device.getSurfaceSupportKHR(index, surface)) {
        presentQueue = index;
      }
      if (graphicsQueue.has_value() && presentQueue.has_value()) {
        break;
      }
    }
    if (!graphicsQueue.has_value() || !presentQueue.has_value()) {
      continue;
    }

    if (!std::ranges::contains_subrange(
            extensions | std::ranges::views::transform([](auto extension) {
              return std::string_view{extension.extensionName};
            }),
            kVkDeviceExtensions |
                std::ranges::views::transform([](auto extensionName) {
                  return std::string_view{extensionName};
                }))) {
      continue;
    };

    selectedDevice = device;
    queueFamilies = {*graphicsQueue, *presentQueue};
    queueFamilyCount = graphicsQueue == presentQueue ? 1 : 2;
    break;
  }
  if (!selectedDevice.has_value()) {
    throw std::runtime_error(std::format("no suitable Vulkan device found"));
  }
  return std::make_tuple(*selectedDevice, queueFamilies, queueFamilyCount);
}

std::tuple<vk::UniqueDevice, vk::Queue, vk::Queue>
createDevice(vk::PhysicalDevice physicalDevice,
             std::array<QueueIndex, 2> queueFamilies,
             uint32_t queueFamilyCount) {
  float queuePriority = 0.0f;
  auto queueInfos = std::to_array({
      vk::DeviceQueueCreateInfo({}, queueFamilies[0], 1, &queuePriority),
      vk::DeviceQueueCreateInfo({}, queueFamilies[1], 1, &queuePriority),
  });
  auto device = physicalDevice.createDeviceUnique(
      vk::DeviceCreateInfo{}
          .setPQueueCreateInfos(queueInfos.data())
          .setQueueCreateInfoCount(queueFamilyCount)
          .setPEnabledLayerNames(kVkLayers)
          .setPEnabledExtensionNames(kVkDeviceExtensions));
  auto graphicsQueue = device->getQueue(queueFamilies[0], 0);
  auto presentQueue = device->getQueue(queueFamilies[1], 0);
  return std::make_tuple(std::move(device), graphicsQueue, presentQueue);
}

GraphicsDevice GraphicsDevice::createFor(const Window &window,
                                         std::string_view appName,
                                         AppVersion appVersion) {
  auto instance = createInstance(appName, appVersion);
  auto physicalDevices = instance->enumeratePhysicalDevices();
  auto surface = createSurface(*instance, window.glfwWindow());
  auto [physicalDevice, queueFamilies, queueFamilyCount] =
      autoselectPhysicalDevice(physicalDevices, *surface);
  auto [device, graphicsQueue, presentQueue] =
      createDevice(physicalDevice, queueFamilies, queueFamilyCount);
  auto workQueue = graphicsQueue;
  auto allocator =
      vma::createAllocatorUnique(vma::AllocatorCreateInfo{}
                                     .setDevice(*device)
                                     .setVulkanApiVersion(vk::ApiVersion13)
                                     .setPhysicalDevice(physicalDevice)
                                     .setInstance(*instance));
  return {std::move(instance), std::move(surface), physicalDevice,
          std::move(device),   queueFamilies,      queueFamilyCount,
          workQueue,           graphicsQueue,      presentQueue,
          std::move(allocator)};
}

void GraphicsDevice::waitIdle() const { _vkDevice->waitIdle(); }
