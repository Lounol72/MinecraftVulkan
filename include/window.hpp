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
  VkExtent2D getExtent() {
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
  }
  bool wasWindowResized() { return frameBufferResized; }
  void resetWindowResizedFlag() { frameBufferResized = false; }

  // Creates a Vulkan surface tied to this window, required by the swapchain.
  void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

private:
  static void frameBufferResizeCallback(GLFWwindow *window, int width,
                                        int height);
  void initWindow();

  int width;
  int height;
  bool frameBufferResized = false;

  std::string windowName;
  GLFWwindow *window;
};
} // namespace mc
