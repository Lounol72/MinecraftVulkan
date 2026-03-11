#include "../include/app.hpp"
#include <GLFW/glfw3.h>
#include <iostream>

namespace mc {
void App::run() {
  while (!window.shouldClose()) {
    glfwPollEvents();
  }
}
} // namespace mc
