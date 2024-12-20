#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <ranges>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnullability-extension"
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.hpp"
#pragma GCC diagnostic pop

namespace views = std::ranges::views;

const vk::ApplicationInfo kApplicationInfo = {
    "Glock Engine",           VK_MAKE_VERSION(0, 1, 0), "Glock Engine",
    VK_MAKE_VERSION(0, 1, 0), vk::ApiVersion13,
};

const auto kVkLayers = std::to_array({"VK_LAYER_KHRONOS_validation"});
const auto kVkDeviceExtensions = std::to_array({vk::KHRSwapchainExtensionName});

struct GLFWWindowDestroyer {
  void operator()(GLFWwindow *ptr) {
    glfwDestroyWindow(ptr);
    glfwTerminate();
  }
};
using UniqueWindow = std::unique_ptr<GLFWwindow, GLFWWindowDestroyer>;

void glfwErrorCallback(int, const char *description) {
  std::println("Error {}", description);
}

int main(void) {
  // Create window
  UniqueWindow window{
      (glfwInit(), glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API),
       glfwCreateWindow(640, 480, "Glock Engine", nullptr, nullptr))};

  // Create Vulkan instance
  uint32_t vkInstanceExtensionCount;
  const char **vkInstanceExtensions =
      glfwGetRequiredInstanceExtensions(&vkInstanceExtensionCount);
  auto instance = vk::createInstanceUnique(vk::InstanceCreateInfo{
      {},
      &kApplicationInfo,
      kVkLayers.size(),
      kVkLayers.data(),
      vkInstanceExtensionCount,
      vkInstanceExtensions,
  });

  // Create Vulkan surface from window
  VkSurfaceKHR rawSurface;
  VkResult rawResult = glfwCreateWindowSurface(instance.get(), window.get(),
                                               nullptr, &rawSurface);
  if (rawResult != (VkResult)vk::Result::eSuccess) {
    std::println(std::cerr,
                 "[ERROR][GLFW] failed to create window. Result code: {}",
                 (int)rawResult);
    return -1;
  }
  vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic> deleter(
      instance.get());
  vk::UniqueSurfaceKHR surface{rawSurface, deleter};

  // Select physical device
  auto physicalDevices = instance->enumeratePhysicalDevices();
  std::optional<vk::PhysicalDevice> selectedDevice;
  std::array<uint32_t, 2> queueFamilies;
  uint32_t queueFamilyCount;
  float queuePriority = 0.0f;
  for (const auto &device : physicalDevices) {
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
          device.getSurfaceSupportKHR(index, *surface)) {
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
    std::println(std::cerr, "[ERROR][VULKAN] no suitable device found");
    return -1;
  }

  // Create Vulkan device
  auto queueInfos = std::to_array({
      vk::DeviceQueueCreateInfo({}, queueFamilies[0], 1, &queuePriority),
      vk::DeviceQueueCreateInfo({}, queueFamilies[1], 1, &queuePriority),
  });
  auto device = selectedDevice->createDeviceUnique(vk::DeviceCreateInfo{
      {},
      queueFamilyCount,
      queueInfos.data(),
      kVkLayers.size(),
      kVkLayers.data(),
      kVkDeviceExtensions.size(),
      kVkDeviceExtensions.data(),
      {},
      {},
  });
  auto graphicsQueue = device->getQueue(queueFamilies[0], 0);
  auto presentQueue = device->getQueue(queueFamilies[1], 0);

  // Create allocator
  auto allocator =
      vma::createAllocatorUnique(vma::AllocatorCreateInfo{}
                                     .setDevice(*device)
                                     .setVulkanApiVersion(vk::ApiVersion13)
                                     .setPhysicalDevice(*selectedDevice)
                                     .setInstance(*instance));

  // Create command pool
  auto commandPool = device->createCommandPoolUnique(
      vk::CommandPoolCreateInfo{{}, queueFamilies[0]});

  // Create swapchain
  auto swapchainCapabilities =
      selectedDevice->getSurfaceCapabilitiesKHR(*surface);
  auto swapchainFormats = selectedDevice->getSurfaceFormatsKHR(*surface);
  auto swapchainPresentModes =
      selectedDevice->getSurfacePresentModesKHR(*surface);

  auto minImageCount = std::max(swapchainCapabilities.minImageCount + 1,
                                swapchainCapabilities.maxImageCount);
  auto swapchainFormat =
      std::ranges::find_if(swapchainFormats, [](vk::SurfaceFormatKHR format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });
  if (swapchainFormat == swapchainFormats.end()) {
    swapchainFormat = swapchainFormats.begin();
  }
  auto swapchainPresentMode =
      std::ranges::contains(swapchainPresentModes, vk::PresentModeKHR::eMailbox)
          ? vk::PresentModeKHR::eMailbox
          : vk::PresentModeKHR::eFifo;

  int glfwWindowWidth;
  int glfwWindowHeight;
  glfwGetFramebufferSize(window.get(), &glfwWindowWidth, &glfwWindowHeight);
  vk::Extent2D swapchainExtent{
      std::clamp((uint32_t)glfwWindowWidth,
                 swapchainCapabilities.minImageExtent.width,
                 swapchainCapabilities.maxImageExtent.width),
      std::clamp((uint32_t)glfwWindowHeight,
                 swapchainCapabilities.minImageExtent.height,
                 swapchainCapabilities.maxImageExtent.height)};

  auto swapchain = device->createSwapchainKHRUnique(vk::SwapchainCreateInfoKHR{
      {},
      *surface,
      minImageCount,
      swapchainFormat->format,
      swapchainFormat->colorSpace,
      swapchainExtent,
      1,
      vk::ImageUsageFlagBits::eColorAttachment,
      queueFamilyCount == 1 ? vk::SharingMode::eExclusive
                            : vk::SharingMode::eConcurrent,
      queueFamilyCount,
      queueFamilies.data(),
      vk::SurfaceTransformFlagBitsKHR::eIdentity,
      vk::CompositeAlphaFlagBitsKHR::eOpaque,
      swapchainPresentMode,
      true});
  auto swapchainImages = device->getSwapchainImagesKHR(*swapchain);
  auto swapchainImageViews =
      swapchainImages | views::transform([&](vk::Image image) {
        return device->createImageViewUnique(vk::ImageViewCreateInfo{
            {},
            image,
            vk::ImageViewType::e2D,
            swapchainFormat->format,
            {},
            vk::ImageSubresourceRange{
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1,
            },
        });
      }) |
      std::ranges::to<std::vector<vk::UniqueImageView>>();

  // Create synchonization primitives
  const size_t maxConcurrentFrames = 2;
  std::array<vk::UniqueFence, maxConcurrentFrames> frameFences;
  std::array<vk::UniqueSemaphore, maxConcurrentFrames> readyImageSemaphores;
  std::array<vk::UniqueSemaphore, maxConcurrentFrames> doneImageSemaphores;
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

  // Create render pass
  auto attachments = std::to_array({
      vk::AttachmentDescription{}
          .setFormat(swapchainFormat->format)
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
  auto subpasses = std::to_array({vk::SubpassDescription{
      {},
      vk::PipelineBindPoint::eGraphics,
      0,
      {},
      1,
      &attachmentsRefs[0],
      {},
      {},
      0,
      {},
  }});
  auto subpassDependencies = std::to_array(
      {vk::SubpassDependency{}
           .setSrcSubpass(vk::SubpassExternal)
           .setDstSubpass(0)
           .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
           .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
           .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)});
  auto renderPass = device->createRenderPassUnique(vk::RenderPassCreateInfo{
      {}, attachments, subpasses, subpassDependencies});

  // Load shaders
  const auto vertShaderPath = "./assets/shaders/white.vert.spv";
  const auto fragShaderPath = "./assets/shaders/white.frag.spv";
  std::ifstream vertShaderFile(vertShaderPath, std::ios::binary);
  std::ifstream fragShaderFile(fragShaderPath, std::ios::binary);
  if (!vertShaderFile.is_open()) {
    std::println(std::cerr, "failed to open file {}", vertShaderPath);
    return -1;
  }
  if (!fragShaderFile.is_open()) {
    std::println(std::cerr, "failed to open file {}", vertShaderPath);
    return -1;
  }
  std::vector<char> vertShader{std::istreambuf_iterator(vertShaderFile), {}};
  std::vector<char> fragShader{std::istreambuf_iterator(fragShaderFile), {}};
  auto vertShaderModule =
      device->createShaderModuleUnique(vk::ShaderModuleCreateInfo{
          {},
          vertShader.size(),
          reinterpret_cast<const uint32_t *>(vertShader.data())});
  auto fragShaderModule =
      device->createShaderModuleUnique(vk::ShaderModuleCreateInfo{
          {},
          fragShader.size(),
          reinterpret_cast<const uint32_t *>(fragShader.data())});

  // Create pipeline
  auto pipelineLayout = device->createPipelineLayoutUnique(
      vk::PipelineLayoutCreateInfo{{}, 0, nullptr, 0, nullptr});
  auto pipelineStages = std::to_array({
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eVertex, *vertShaderModule, "main"},
      vk::PipelineShaderStageCreateInfo{
          {}, vk::ShaderStageFlagBits::eFragment, *fragShaderModule, "main"},
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
  vk::Viewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(swapchainExtent.width),
                        static_cast<float>(swapchainExtent.height),
                        0.0f,
                        1.0f};
  vk::Rect2D scissor{{0, 0}, swapchainExtent};
  vk::PipelineViewportStateCreateInfo viewportInfo{
      {}, 1, &viewport, 1, &scissor};
  auto rasterizationState =
      vk::PipelineRasterizationStateCreateInfo{}.setLineWidth(1.0f);
  vk::PipelineMultisampleStateCreateInfo multisampleState{};
  auto colorBlendingAttachments = std::to_array({
      vk::PipelineColorBlendAttachmentState{
          false,
          vk::BlendFactor::eZero,
          vk::BlendFactor::eZero,
          vk::BlendOp::eAdd,
          vk::BlendFactor::eZero,
          vk::BlendFactor::eZero,
          vk::BlendOp::eAdd,
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
      },
  });
  vk::PipelineColorBlendStateCreateInfo colorBlendState{
      {}, false, {}, colorBlendingAttachments};
  auto pipeline = device
                      ->createGraphicsPipelineUnique(
                          {}, vk::GraphicsPipelineCreateInfo{}
                                  .setStages(pipelineStages)
                                  .setPVertexInputState(&vertexInputInfo)
                                  .setPInputAssemblyState(&inputAssemblyInfo)
                                  .setPViewportState(&viewportInfo)
                                  .setPRasterizationState(&rasterizationState)
                                  .setPMultisampleState(&multisampleState)
                                  .setPColorBlendState(&colorBlendState)
                                  .setLayout(*pipelineLayout)
                                  .setRenderPass(*renderPass))
                      .value;

  // Create framebuffers
  auto framebuffers = swapchainImageViews |
                      views::transform([&](const vk::UniqueImageView &view) {
                        return device->createFramebufferUnique(
                            vk::FramebufferCreateInfo{}
                                .setRenderPass(*renderPass)
                                .setAttachments(*view)
                                .setWidth(swapchainExtent.width)
                                .setHeight(swapchainExtent.height)
                                .setLayers(1));
                      }) |
                      std::ranges::to<std::vector>();

  // Load vertex buffer
  auto vertices = std::to_array({glm::vec4(-1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(1.0, 1.0, 0.0, 1.0),
                                 glm::vec4(0.0, -1.0, 0.0, 1.0)});
  auto vertexBuffer = allocator->createBufferUnique(
      vk::BufferCreateInfo{}
          .setUsage(vk::BufferUsageFlagBits::eVertexBuffer)
          .setSize(vertices.size() * sizeof(glm::vec4)),
      vma::AllocationCreateInfo{}.setUsage(vma::MemoryUsage::eCpuToGpu));
  void *vertexBufferData = allocator->mapMemory(*vertexBuffer.second);
  std::memcpy(vertexBufferData, vertices.data(),
              vertices.size() * sizeof(glm::vec4));
  allocator->unmapMemory(*vertexBuffer.second);

  // Record command buffers.
  auto commandBuffers = device->allocateCommandBuffers(
      vk::CommandBufferAllocateInfo{}
          .setCommandPool(*commandPool)
          .setCommandBufferCount((uint32_t)framebuffers.size()));
  for (auto [commandBuffer, framebuffer] :
       views::zip(commandBuffers, framebuffers)) {
    commandBuffer.begin(vk::CommandBufferBeginInfo{});
    vk::ClearValue clearValues = {
        vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};
    commandBuffer.beginRenderPass(
        vk::RenderPassBeginInfo{}
            .setRenderPass(*renderPass)
            .setFramebuffer(*framebuffer)
            .setClearValues(clearValues)
            .setRenderArea(vk::Rect2D{{0, 0}, swapchainExtent}),
        vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
    commandBuffer.bindVertexBuffers(0, *vertexBuffer.first, {0});
    commandBuffer.draw(vertices.size(), 1, 0, 0);
    commandBuffer.endRenderPass();
    commandBuffer.end();
  }

  // Game loop
  uint32_t currentFrame = 0;
  std::vector<vk::Fence> imageFences;
  imageFences.resize(framebuffers.size());
  while (!glfwWindowShouldClose(window.get())) {
    auto frameFence = *frameFences[currentFrame];
    auto readyImageSemaphore = *readyImageSemaphores[currentFrame];
    auto doneImageSemaphore = *doneImageSemaphores[currentFrame];
    std::ignore = device->waitForFences(frameFence, true,
                                        std::numeric_limits<uint64_t>::max());
    uint32_t imageIndex =
        device
            ->acquireNextImageKHR(*swapchain,
                                  std::numeric_limits<uint64_t>::max(),
                                  readyImageSemaphore)
            .value;
    auto &imageFence = imageFences[imageIndex];
    if (imageFence != nullptr) {
      std::ignore = device->waitForFences(imageFence, true,
                                          std::numeric_limits<uint64_t>::max());
    }
    imageFence = frameFence;
    device->resetFences(frameFence);
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
                           .setSwapchains(*swapchain)
                           .setImageIndices(imageIndex);
    std::ignore = presentQueue.presentKHR(presentInfo);
    currentFrame = (currentFrame + 1) % maxConcurrentFrames;
    glfwPollEvents();
  }
  device->waitIdle();
  return 0;
}
