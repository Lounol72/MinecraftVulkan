#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include "window.hpp"

namespace mc {

// Top-level entry point: owns all Vulkan resources and drives the event loop.
// Initialization order matters — Window must exist before Device, Device before Pipeline.
class App {
public:
  static constexpr int WIDTH = 800;
  static constexpr int HEIGHT = 600;

  void run();

private:
  Window window{WIDTH, HEIGHT, "Hello Vulkan!"};
  Device device{window};
  Pipeline pipeline{device, "shaders/shader.vert.spv",
                    "shaders/shader.frag.spv",
                    Pipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
};
} // namespace mc
