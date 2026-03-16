#pragma once

#include "device.hpp"
#include "model.hpp"
#include "pipeline.hpp"
#include "swap_chain.hpp"
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
  void loadModels();
  void createPipelineLayout();
  void createPipeline();
  void createCommandBuffers();
  void freeCommandBuffers();
  void drawFrame();
  void recreateSwapChain();
  void recordCommandBuffer(int imageIndex);

  Window window{WIDTH, HEIGHT, "Hello Vulkan!"};
  Device device{window};
  std::unique_ptr<SwapChain> swapChain;
  std::unique_ptr<Pipeline> pipeline;
  VkPipelineLayout pipelineLayout;
  std::vector<VkCommandBuffer> commandBuffers;
  std::unique_ptr<Model> model;
};
} // namespace mc
