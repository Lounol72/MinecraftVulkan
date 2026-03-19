#pragma once

#include "descriptors.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "window.hpp"

// std
#include <memory>
#include <vector>

namespace mc {

// Top-level entry point: owns all Vulkan resources and drives the event loop.
// Initialization order matters — Window must exist before Device, Device
// before Pipeline.
class App {
public:
  static constexpr int WIDTH = 800;
  static constexpr int HEIGHT = 600;

  App();
  ~App();

  App(const App &) = delete;
  App &operator=(const App &) = delete;

  void run();

private:
  void loadGameObjects();

  Window window{WIDTH, HEIGHT, "Hello Vulkan!"};
  Device device{window};
  Renderer renderer{window, device};

  std::unique_ptr<DescriptorPool> globalPool{};
  std::vector<GameObject> gameObjects;
};
} // namespace mc
