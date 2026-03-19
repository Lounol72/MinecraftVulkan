#pragma once

#include "camera.hpp"

// libs

#include <vulkan/vulkan.h>

namespace mc {
struct FrameInfo {
  int frameIndex;
  float frameTime;
  VkCommandBuffer commandBuffer;
  Camera &camera;
  VkDescriptorSet globalDescriptorSet;
};
} // namespace mc
