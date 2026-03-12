#pragma once

#include <string>
#include <vulkan/vulkan_core.h>
// GLFW_INCLUDE_VULKAN makes GLFW pull in vulkan.h automatically
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace mc {

// Thin RAII wrapper around a GLFWwindow.
// Non-copyable: each instance maps to exactly one OS window.
class Window {
public:
  Window(int w, int h, std::string name);
  ~Window();
  Window(const Window &) = delete;
  Window &operator=(const Window &) = delete;

  bool shouldClose() { return glfwWindowShouldClose(window); }

  // Creates a Vulkan surface tied to this window, required by the swapchain.
  void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

private:
  void initWindow();

  const int width;
  const int height;
  std::string windowName;
  GLFWwindow *window;
};
} // namespace mc
