#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vulkan/vulkan.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnullability-extension"
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.hpp"
#pragma GCC diagnostic pop

#include "window.hpp"

namespace views = std::ranges::views;

using QueueIndex = uint32_t;

const size_t kMaxConcurrentFrames = 2;
const vk::ApplicationInfo kApplicationInfo = {
    "Glock Engine",           VK_MAKE_VERSION(0, 1, 0), "Glock Engine",
    VK_MAKE_VERSION(0, 1, 0), vk::ApiVersion13,
};
const auto kVkLayers = std::to_array({"VK_LAYER_KHRONOS_validation"});
const auto kVkDeviceExtensions = std::to_array({vk::KHRSwapchainExtensionName});
const auto kVertShaderPath = "./assets/shaders/white.vert.spv";
const auto kFragShaderPath = "./assets/shaders/white.frag.spv";

vk::UniqueInstance createInstance() {
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

std::tuple<vk::PhysicalDevice, std::array<QueueIndex, 2>, uint32_t>
autoselectPhysicalDevice(const vk::Instance instance,
                         const vk::SurfaceKHR surface) {
  std::optional<vk::PhysicalDevice> selectedDevice;
  std::array<uint32_t, 2> queueFamilies;
  uint32_t queueFamilyCount;
  auto physicalDevices = instance.enumeratePhysicalDevices();
  std::println("Available devices:");
  for (auto [i, device] : physicalDevices | views::enumerate) {
    // 1. Needs a Graphics queue and a Present queue;
    // 2. Needs to support all required device extensions
    auto properties = device.getProperties();
    auto extensions = device.enumerateDeviceExtensionProperties();
    auto allQueueFamilies = device.getQueueFamilyProperties();

    std::println("{}. {} [{}:{}]", i, properties.deviceName.data(),
                 properties.vendorID, properties.deviceID);
    std::println("- Extensions:");
    for (auto extension : extensions) {
      std::println("  - {}", extension.extensionName.data());
    }

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

std::tuple<vk::UniqueSwapchainKHR, std::vector<vk::Image>,
           std::vector<vk::UniqueImageView>, vk::SurfaceFormatKHR, vk::Extent2D>
createSwapchain(vk::SurfaceKHR surface, GLFWwindow *window,
                vk::PhysicalDevice physicalDevice, vk::Device device,
                std::span<QueueIndex> queueFamilies,
                std::optional<vk::SwapchainKHR> oldSwapchain = {}) {

  auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
  auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
  auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

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

  int glfwWindowWidth;
  int glfwWindowHeight;
  glfwGetFramebufferSize(window, &glfwWindowWidth, &glfwWindowHeight);
  vk::Extent2D extent{
      std::clamp((uint32_t)glfwWindowWidth, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width),
      std::clamp((uint32_t)glfwWindowHeight, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height)};

  auto sharingMode = queueFamilies.size() == 1 ? vk::SharingMode::eExclusive
                                               : vk::SharingMode::eConcurrent;
  auto swapchain = device.createSwapchainKHRUnique(
      vk::SwapchainCreateInfoKHR{}
          .setSurface(surface)
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
          .setOldSwapchain(oldSwapchain.value_or(nullptr)));
  auto images = device.getSwapchainImagesKHR(*swapchain);
  auto imageViews = images | views::transform([&](vk::Image image) {
                      return device.createImageViewUnique(
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
  return std::make_tuple(std::move(swapchain), images, std::move(imageViews),
                         *format, extent);
}

vk::UniqueRenderPass createRenderPass(vk::Device device,
                                      vk::Format swapchainFormat) {
  auto attachments = std::to_array({
      vk::AttachmentDescription{}
          .setFormat(swapchainFormat)
          .setLoadOp(vk::AttachmentLoadOp::eClear)
          .setStoreOp(vk::AttachmentStoreOp::eStore)
          .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
          .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
          .setInitialLayout(vk::ImageLayout::eUndefined)
          .setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
  });
  auto attachmentsRefs = std::to_array({
      vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal},
  });
  auto subpasses = std::to_array({
      vk::SubpassDescription{}
          .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
          .setColorAttachments(attachmentsRefs[0]),
  });
  auto subpassDependencies = std::to_array({
      vk::SubpassDependency{}
          .setSrcSubpass(vk::SubpassExternal)
          .setDstSubpass(0)
          .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
          .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite),
  });
  return device.createRenderPassUnique(vk::RenderPassCreateInfo{
      {}, attachments, subpasses, subpassDependencies});
}

struct RenderContext {
  vk::UniqueSwapchainKHR swapchain;
  vk::Extent2D extent;
  vk::SurfaceFormatKHR format;
  std::vector<vk::Image> images;
  std::vector<vk::UniqueImageView> imageViews;
  vk::UniqueRenderPass renderPass;
  std::vector<vk::UniqueFramebuffer> framebuffers;
  std::vector<vk::CommandBuffer> commandBuffers;

  inline size_t imageCount() const { return images.size(); }
};

RenderContext
createRenderContext(vk::SurfaceKHR surface, GLFWwindow *window,
                    vk::PhysicalDevice physicalDevice, vk::Device device,
                    std::span<QueueIndex> queueFamilies,
                    vk::CommandPool commandPool,
                    std::optional<vk::SwapchainKHR> oldSwapchain = {}) {
  auto [swapchain, swapchainImages, swapchainImageViews, swapchainFormat,
        swapchainExtent] = createSwapchain(surface, window, physicalDevice,
                                           device, queueFamilies, oldSwapchain);
  auto renderPass = createRenderPass(device, swapchainFormat.format);
  auto framebuffers = swapchainImageViews |
                      views::transform([&](const vk::UniqueImageView &view) {
                        return device.createFramebufferUnique(
                            vk::FramebufferCreateInfo{}
                                .setRenderPass(*renderPass)
                                .setAttachments(*view)
                                .setWidth(swapchainExtent.width)
                                .setHeight(swapchainExtent.height)
                                .setLayers(1));
                      }) |
                      std::ranges::to<std::vector>();
  auto commandBuffers = device.allocateCommandBuffers(
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(commandPool)
          .setCommandBufferCount((uint32_t)framebuffers.size()));
  return {.swapchain = std::move(swapchain),
          .extent = swapchainExtent,
          .format = swapchainFormat,
          .images = swapchainImages,
          .imageViews = std::move(swapchainImageViews),
          .renderPass = std::move(renderPass),
          .framebuffers = std::move(framebuffers),
          .commandBuffers = commandBuffers};
}

vk::UniqueShaderModule loadShaderFromPath(const std::string_view path,
                                          vk::Device device) {
  std::ifstream file(path.data(), std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error(std::format("failed to open file {}", path));
  }
  std::vector<char> shaderData{std::istreambuf_iterator(file), {}};
  return device.createShaderModuleUnique(
      vk::ShaderModuleCreateInfo{}
          .setCodeSize(shaderData.size())
          .setPCode(reinterpret_cast<const uint32_t *>(shaderData.data())));
}

std::tuple<vk::UniquePipeline, vk::UniquePipelineLayout,
           std::array<vk::UniqueShaderModule, 2>>
createWhiteGraphicsPipeline(vk::Extent2D swapchainExtent,
                            vk::RenderPass renderPass, vk::Device device) {
  auto shaderModules = std::to_array({
      loadShaderFromPath(kVertShaderPath, device),
      loadShaderFromPath(kFragShaderPath, device),
  });
  auto pipelineLayout =
      device.createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{});
  auto pipelineStages = std::to_array({
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eVertex, *shaderModules[0], "main"},
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eFragment, *shaderModules[1], "main"},
  });
  auto vertexBindings = std::to_array({
      vk::VertexInputBindingDescription{0, sizeof(glm::vec4)},
  });
  auto vertexAttributes = std::to_array({
      vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32A32Sfloat,
                                          0},
  });
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      {}, vertexBindings, vertexAttributes};
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
      {}, vk::PrimitiveTopology::eTriangleList};
  vk::Viewport viewport{
      0.0f,
      0.0f,
      static_cast<float>(swapchainExtent.width),
      static_cast<float>(swapchainExtent.height),
      0.0f,
      1.0f,
  };
  vk::Rect2D scissor{{0, 0}, swapchainExtent};
  vk::PipelineViewportStateCreateInfo viewportInfo{{}, viewport, scissor};
  auto rasterizationState =
      vk::PipelineRasterizationStateCreateInfo{}.setLineWidth(1.0f);
  vk::PipelineMultisampleStateCreateInfo multisampleState{};
  auto colorBlendingAttachments = std::to_array({
      vk::PipelineColorBlendAttachmentState{}
          .setBlendEnable(false)
          .setColorWriteMask(
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
  });
  auto colorBlendState = vk::PipelineColorBlendStateCreateInfo{}.setAttachments(
      colorBlendingAttachments);
  auto pipeline = device
                      .createGraphicsPipelineUnique(
                          {}, vk::GraphicsPipelineCreateInfo{}
                                  .setStages(pipelineStages)
                                  .setPVertexInputState(&vertexInputInfo)
                                  .setPInputAssemblyState(&inputAssemblyInfo)
                                  .setPViewportState(&viewportInfo)
                                  .setPRasterizationState(&rasterizationState)
                                  .setPMultisampleState(&multisampleState)
                                  .setPColorBlendState(&colorBlendState)
                                  .setLayout(*pipelineLayout)
                                  .setRenderPass(renderPass))
                      .value;
  return std::make_tuple(std::move(pipeline), std::move(pipelineLayout),
                         std::move(shaderModules));
}

template <class T, size_t Nm>
std::tuple<vma::UniqueBuffer, vma::UniqueAllocation>
createImmutableBuffer(const std::array<T, Nm> &data, vk::BufferUsageFlags usage,
                      vk::CommandPool commandPool, vk::Queue queue,
                      vk::Device device, vma::Allocator allocator) {
  const size_t size = sizeof(data);

  auto finalBuffer = allocator.createBufferUnique(
      vk::BufferCreateInfo{}
          .setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
          .setSize(size),
      vma::AllocationCreateInfo{}.setUsage(vma::MemoryUsage::eGpuOnly));

  auto stagingBuffer = allocator.createBufferUnique(
      vk::BufferCreateInfo{}
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSize(size),
      vma::AllocationCreateInfo{}.setUsage(vma::MemoryUsage::eCpuToGpu));
  T *stagingPtr =
      reinterpret_cast<T *>(allocator.mapMemory(*stagingBuffer.second));
  std::ranges::copy(data, stagingPtr);
  allocator.unmapMemory(*stagingBuffer.second);

  auto commandBuffer =
      device.allocateCommandBuffers(vk::CommandBufferAllocateInfo{}
                                        .setCommandPool(commandPool)
                                        .setCommandBufferCount(1))[0];
  commandBuffer.begin(vk::CommandBufferBeginInfo{
      vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  commandBuffer.copyBuffer(*stagingBuffer.first, *finalBuffer.first,
                           vk::BufferCopy{}.setSize(size));
  commandBuffer.end();
  queue.submit(vk::SubmitInfo{}.setCommandBuffers(commandBuffer));
  queue.waitIdle();
  device.freeCommandBuffers(commandPool, commandBuffer);

  return std::make_tuple(std::move(finalBuffer.first),
                         std::move(finalBuffer.second));
}

void recordRenderCommandBuffer(vk::CommandBuffer commandBuffer,
                               vk::Extent2D extent, vk::RenderPass renderPass,
                               vk::Pipeline pipeline, vk::Buffer vertexBuffer,
                               uint32_t verticeCount,
                               vk::Framebuffer framebuffer) {
  commandBuffer.begin(vk::CommandBufferBeginInfo{});
  vk::ClearValue clearValues = {
      vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
  commandBuffer.beginRenderPass(vk::RenderPassBeginInfo{}
                                    .setRenderPass(renderPass)
                                    .setFramebuffer(framebuffer)
                                    .setClearValues(clearValues)
                                    .setRenderArea(vk::Rect2D{{0, 0}, extent}),
                                vk::SubpassContents::eInline);
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
  commandBuffer.bindVertexBuffers(0, vertexBuffer, {0});
  commandBuffer.draw(verticeCount, 1, 0, 0);
  commandBuffer.endRenderPass();
  commandBuffer.end();
}

bool render(
    uint32_t frame, std::span<vk::CommandBuffer> commandBuffers,
    std::span<vk::UniqueFence, kMaxConcurrentFrames> frameFences,
    std::span<vk::UniqueSemaphore, kMaxConcurrentFrames> readyImageSemaphores,
    std::span<vk::UniqueSemaphore, kMaxConcurrentFrames> doneImageSemaphores,
    std::span<vk::Fence> imageFences, vk::Device device,
    vk::Queue graphicsQueue, vk::Queue presentQueue,
    vk::SwapchainKHR swapchain) {
  auto renderFrame = frame % kMaxConcurrentFrames;
  auto frameFence = *frameFences[renderFrame];
  auto readyImageSemaphore = *readyImageSemaphores[renderFrame];
  auto doneImageSemaphore = *doneImageSemaphores[renderFrame];
  std::ignore = device.waitForFences(frameFence, true,
                                     std::numeric_limits<uint64_t>::max());
  uint32_t imageIndex =
      device
          .acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(),
                               readyImageSemaphore)
          .value;
  auto &imageFence = imageFences[imageIndex];
  if (imageFence != nullptr) {
    std::ignore = device.waitForFences(imageFence, true,
                                       std::numeric_limits<uint64_t>::max());
  }
  imageFence = frameFence;
  device.resetFences(frameFence);
  auto commandBuffer = commandBuffers[imageIndex];
  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;
  auto submitInfo = vk::SubmitInfo{}
                        .setWaitSemaphores(readyImageSemaphore)
                        .setWaitDstStageMask(waitStage)
                        .setCommandBuffers(commandBuffer)
                        .setSignalSemaphores(doneImageSemaphore);
  graphicsQueue.submit(submitInfo, frameFence);
  auto presentInfo = vk::PresentInfoKHR{}
                         .setWaitSemaphores(doneImageSemaphore)
                         .setSwapchains(swapchain)
                         .setImageIndices(imageIndex);
  try {
    auto result = presentQueue.presentKHR(presentInfo);
    return result != vk::Result::eSuccess;
  } catch (vk::OutOfDateKHRError) {
    return true;
  }
}

int main(void) {
  auto window = Window::create("Glock Engine", 640, 480);
  auto instance = createInstance();
  auto surface = createSurface(*instance, window.glfwWindow());
  auto [physicalDevice, queueFamilies, queueFamilyCount] =
      autoselectPhysicalDevice(*instance, *surface);
  auto [device, graphicsQueue, presentQueue] =
      createDevice(physicalDevice, queueFamilies, queueFamilyCount);
  auto workQueue = graphicsQueue;
  auto allocator =
      vma::createAllocatorUnique(vma::AllocatorCreateInfo{}
                                     .setDevice(*device)
                                     .setVulkanApiVersion(vk::ApiVersion13)
                                     .setPhysicalDevice(physicalDevice)
                                     .setInstance(*instance));
  auto renderCommandPool = device->createCommandPoolUnique(
      vk::CommandPoolCreateInfo{{}, queueFamilies[0]});
  auto workCommandPool =
      device->createCommandPoolUnique(vk::CommandPoolCreateInfo{
          vk::CommandPoolCreateFlagBits::eTransient, queueFamilies[0]});
  std::array<vk::UniqueFence, kMaxConcurrentFrames> frameFences;
  std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> readyImageSemaphores;
  std::array<vk::UniqueSemaphore, kMaxConcurrentFrames> doneImageSemaphores;
  std::ranges::generate(frameFences, [&device]() {
    return device->createFenceUnique(
        vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled));
  });
  std::ranges::generate(readyImageSemaphores, [&device]() {
    return device->createSemaphoreUnique({});
  });
  std::ranges::generate(doneImageSemaphores, [&device]() {
    return device->createSemaphoreUnique({});
  });
  auto renderContext = createRenderContext(
      *surface, window.glfwWindow(), physicalDevice, *device,
      std::span{queueFamilies.data(), queueFamilyCount}, *renderCommandPool);
  auto [pipeline, pipelineLayout, shaderModules] = createWhiteGraphicsPipeline(
      renderContext.extent, *renderContext.renderPass, *device);

  // Load vertex buffer
  auto vertices = std::to_array({glm::vec4(-1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(0.0, -1.0, 0.0, 1.0)});
  auto [vertexBuffer, vertexBufferAllocation] =
      createImmutableBuffer(vertices, vk::BufferUsageFlagBits::eVertexBuffer,
                            *workCommandPool, workQueue, *device, *allocator);

  // Record command buffers.
  for (auto [commandBuffer, framebuffer] :
       views::zip(renderContext.commandBuffers, renderContext.framebuffers)) {
    recordRenderCommandBuffer(commandBuffer, renderContext.extent,
                              *renderContext.renderPass, *pipeline,
                              *vertexBuffer, vertices.size(), *framebuffer);
  }

  // Game loop
  uint32_t currentFrame = 0;
  std::vector<vk::Fence> imageFences;
  imageFences.resize(renderContext.imageCount());
  while (!window.shouldClose()) {
    bool needsSwapchainRecreation =
        render(currentFrame, renderContext.commandBuffers, frameFences,
               readyImageSemaphores, doneImageSemaphores, imageFences, *device,
               graphicsQueue, presentQueue, *renderContext.swapchain);
    if (needsSwapchainRecreation) {
      window.waitForValidDimensions();
      device->waitIdle();
      renderContext =
          createRenderContext(*surface, window.glfwWindow(), physicalDevice,
                              *device, {queueFamilies.data(), queueFamilyCount},
                              *renderCommandPool, *renderContext.swapchain);
    }
    currentFrame++;
    glfwPollEvents();
  }
  device->waitIdle();
  return 0;
}
