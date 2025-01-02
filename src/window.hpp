#pragma once

#include <memory>
#include <string_view>

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Window {
  Window(GLFWwindow *glfwWindow) : _glfwWindow(glfwWindow) {}
  static Window create(std::string_view title, uint32_t width, uint32_t height);

  bool shouldClose() const;
  void waitForValidDimensions() const;
  inline vk::Extent2D size() const {
    int width, height;
    glfwGetFramebufferSize(_glfwWindow.get(), &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
  }

  inline GLFWwindow *glfwWindow() const { return _glfwWindow.get(); }

private:
  struct GLFWWindowDestroyer {
    void operator()(GLFWwindow *ptr) {
      glfwDestroyWindow(ptr);
      glfwTerminate();
    }
  };
  std::unique_ptr<GLFWwindow, GLFWWindowDestroyer> _glfwWindow;
};
