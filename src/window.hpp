#pragma once

#include <memory>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Window {
  Window(GLFWwindow *glfwWindow) : _glfwWindow(glfwWindow) {}
  static Window create(std::string_view title, uint32_t width, uint32_t height);

  bool shouldClose() const;
  void waitForValidDimensions() const;

  GLFWwindow *glfwWindow() const;

private:
  struct GLFWWindowDestroyer {
    void operator()(GLFWwindow *ptr) {
      glfwDestroyWindow(ptr);
      glfwTerminate();
    }
  };
  std::unique_ptr<GLFWwindow, GLFWWindowDestroyer> _glfwWindow;
};
