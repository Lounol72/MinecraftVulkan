#pragma once

#include "device.hpp"
#include "frame_info.hpp"
#include "game_object.hpp"
#include "pipeline.hpp"

// std
#include <memory>
#include <vector>

namespace mc {

  // Top-level entry point: owns all Vulkan resources and drives the event loop.
  // Initialization order matters — Window must exist before Device, Device
  // before Pipeline.
  class PointLightSystem {
  public:
    PointLightSystem(Device               &device,
                     VkRenderPass          renderPass,
                     VkDescriptorSetLayout globalSetLayout);
    ~PointLightSystem();

    PointLightSystem(const PointLightSystem &)            = delete;
    PointLightSystem &operator=(const PointLightSystem &) = delete;

    void render(FrameInfo &frameInfo);

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    Device &device;

    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;
  };
} // namespace mc
