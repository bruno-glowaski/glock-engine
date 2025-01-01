#pragma once

#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.hpp>

#define MAKE_VERSION(major, minor, patch) VK_MAKE_VERSION(major, minor, patch)

using AppVersion = uint32_t;

using QueueIndex = uint32_t;

struct Window;

struct GraphicsDevice {
  GraphicsDevice(vk::UniqueInstance instance, vk::UniqueSurfaceKHR surface,
                 vk::PhysicalDevice physicalDevice, vk::UniqueDevice device,
                 std::array<QueueIndex, 2> queueFamilies,
                 uint32_t queueFamilyCount, vk::Queue workQueue,
                 vk::Queue graphicsQueue, vk::Queue presentQueue,
                 vma::UniqueAllocator _allocator)
      : _vkInstance(std::move(instance)), _vkSurface(std::move(surface)),
        _vkPhysicalDevice(physicalDevice), _vkDevice(std::move(device)),
        _queueFamilies(queueFamilies), _queueFamilyCount(queueFamilyCount),
        _workQueue(workQueue), _graphicsQueue(graphicsQueue),
        _presentQueue(presentQueue), _vmaAllocator(std::move(_allocator)) {}

  static GraphicsDevice createFor(const Window &window,
                                  std::string_view appName,
                                  AppVersion appVersion);

  inline vk::SurfaceKHR vkSurface() const { return _vkSurface.get(); }
  inline vk::Device vkDevice() const { return _vkDevice.get(); }
  inline vk::PhysicalDevice vkPhysicalDevice() const {
    return _vkPhysicalDevice;
  }
  inline vma::Allocator vmaAllocator() const { return _vmaAllocator.get(); }
  inline std::span<const QueueIndex> queueFamilies() const {
    return {_queueFamilies.data(), _queueFamilyCount};
  }
  inline QueueIndex graphicsQueueIndex() const { return _queueFamilies[0]; }
  inline QueueIndex workQueueIndex() const { return _queueFamilies[0]; }
  inline QueueIndex presentQueueIndex() const { return _queueFamilies[1]; }
  inline vk::Queue workQueue() const { return _workQueue; }
  inline vk::Queue graphicsQueue() const { return _graphicsQueue; }
  inline vk::Queue presentQueue() const { return _presentQueue; }

  void waitIdle() const;

private:
  vk::UniqueInstance _vkInstance;
  vk::UniqueSurfaceKHR _vkSurface;
  vk::PhysicalDevice _vkPhysicalDevice;
  vk::UniqueDevice _vkDevice;
  std::array<QueueIndex, 2> _queueFamilies;
  uint32_t _queueFamilyCount;
  vk::Queue _workQueue;
  vk::Queue _graphicsQueue;
  vk::Queue _presentQueue;
  vma::UniqueAllocator _vmaAllocator;
};
