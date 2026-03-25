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
  class SimpleRenderSystem {
  public:
    SimpleRenderSystem(Device &device);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const SimpleRenderSystem &)            = delete;
    SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

    void renderGameObjects(FrameInfo &frameInfo);

  private:
    std::vector<GameObject *> buildRenderList(FrameInfo &frameInfo);
    void renderDepthPrePass(FrameInfo &frameInfo, const std::vector<GameObject *> &renderables);
    void renderColorPass(FrameInfo &frameInfo, const std::vector<GameObject *> &renderables);

    Device &device;
  };
} // namespace mc
