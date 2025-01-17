#pragma once

#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.hpp>

#define MAKE_VERSION(major, minor, patch) VK_MAKE_VERSION(major, minor, patch)

using AppVersion = uint32_t;

using QueueIndex = uint32_t;

struct Window;

struct GraphicsDevice {
  GraphicsDevice(vk::UniqueInstance vkInstance, vk::UniqueSurfaceKHR vkSurface,
                 vk::PhysicalDevice vkPhysicalDevice, vk::UniqueDevice vkDevice,
                 vk::Format depthFormat,
                 std::array<QueueIndex, 2> queueFamilies,
                 uint32_t queueFamilyCount, vk::Queue workQueue,
                 vk::Queue graphicsQueue, vk::Queue presentQueue,
                 vma::UniqueAllocator vmaAllocator,
                 vk::UniqueCommandPool workCommandPool)
      : _vkInstance(std::move(vkInstance)), _vkSurface(std::move(vkSurface)),
        _vkPhysicalDevice(vkPhysicalDevice), _vkDevice(std::move(vkDevice)),
        _depthFormat(depthFormat), _queueFamilies(queueFamilies),
        _queueFamilyCount(queueFamilyCount), _workQueue(workQueue),
        _graphicsQueue(graphicsQueue), _presentQueue(presentQueue),
        _vmaAllocator(std::move(vmaAllocator)),
        _workCommandPool(std::move(workCommandPool)) {}

  static GraphicsDevice createFor(const Window &window,
                                  std::string_view appName,
                                  AppVersion appVersion);

  inline vk::SurfaceKHR vkSurface() const { return _vkSurface.get(); }
  inline vk::Device vkDevice() const { return _vkDevice.get(); }
  inline vk::PhysicalDevice vkPhysicalDevice() const {
    return _vkPhysicalDevice;
  }
  inline vk::Format depthFormat() const { return _depthFormat; }
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

  vk::UniqueCommandPool
  createGraphicsCommandPool(vk::CommandPoolCreateFlags flags) const;
  void waitIdle() const;
  template <std::invocable<vk::CommandBuffer> TCommandBuilder>
  void runOneTimeWork(TCommandBuilder buildFn) const;

private:
  vk::UniqueInstance _vkInstance;
  vk::UniqueSurfaceKHR _vkSurface;
  vk::PhysicalDevice _vkPhysicalDevice;
  vk::UniqueDevice _vkDevice;
  vk::Format _depthFormat;
  std::array<QueueIndex, 2> _queueFamilies;
  uint32_t _queueFamilyCount;
  vk::Queue _workQueue;
  vk::Queue _graphicsQueue;
  vk::Queue _presentQueue;
  vma::UniqueAllocator _vmaAllocator;
  vk::UniqueCommandPool _workCommandPool;
};
