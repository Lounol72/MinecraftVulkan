#include "../include/window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace mc {
Window::Window(int w, int h, std::string name)
    : width{w}, height{h}, windowName{name} {
  initWindow();
}
Window::~Window() {
  glfwDestroyWindow(window);
  glfwTerminate();
}
void Window::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  window =
      glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);
}
void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
  if (glfwCreateWindowSurface(instance, window, nullptr, surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface.");
  }
}
void Window::frameBufferResizeCallback(GLFWwindow *newWindow, int width,
                                       int height) {
  auto window = reinterpret_cast<Window *>(glfwGetWindowUserPointer(newWindow));
  window->frameBufferResized = true;
  window->width = width;
  window->height = height;
}
} // namespace mc
