#pragma once

#include "camera.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "pipeline.hpp"

// std
#include <memory>
#include <vector>

namespace mc {

// Top-level entry point: owns all Vulkan resources and drives the event loop.
// Initialization order matters — Window must exist before Device, Device
// before Pipeline.
class SimpleRenderSystem {
public:
  SimpleRenderSystem(Device &device, VkRenderPass renderPass);
  ~SimpleRenderSystem();

  SimpleRenderSystem(const SimpleRenderSystem &) = delete;
  SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

  void renderGameObjects(VkCommandBuffer commandBuffer,
                         std::vector<GameObject> &gameObjects,
                         const Camera &camera);

private:
  void createPipelineLayout();
  void createPipeline(VkRenderPass renderPass);

  Device &device;

  std::unique_ptr<Pipeline> pipeline;
  VkPipelineLayout pipelineLayout;
};
} // namespace mc
