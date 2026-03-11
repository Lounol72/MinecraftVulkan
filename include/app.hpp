#pragma once

#include "pipeline.hpp"
#include "window.hpp"

namespace mc {
class App {
public:
  static constexpr int WIDTH = 800;
  static constexpr int HEIGHT = 600;

  void run();

private:
  Window window{WIDTH, HEIGHT, "Hello Vulkan!"};
  Pipeline pipeline{"shaders/shader.vert.spv", "shaders/shader.frag.spv"};
};
} // namespace mc
