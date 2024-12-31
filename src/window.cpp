#include "window.hpp"

Window Window::create(std::string_view title, uint32_t width, uint32_t height) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  auto rawWindow =
      glfwCreateWindow(static_cast<int>(width), static_cast<int>(height),
                       std::string(title).c_str(), nullptr, nullptr);
  return {rawWindow};
}

bool Window::shouldClose() const { return glfwWindowShouldClose(glfwWindow()); }
void Window::waitForValidDimensions() const {
  int width = 0, height = 0;
  glfwGetFramebufferSize(glfwWindow(), &width, &height);
  while (width == 0 || height == 0) {
    glfwWaitEvents();
    glfwGetFramebufferSize(glfwWindow(), &width, &height);
  }
}

GLFWwindow *Window::glfwWindow() const { return _glfwWindow.get(); }
