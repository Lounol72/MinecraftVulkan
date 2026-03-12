#include "../include/app.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace mc {
App::App() {
  createPipelineLayout();
  createPipeline();
  createCommandBuffers();
}
App::~App() {
  vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
}
void App::run() {
  while (!window.shouldClose()) {
    glfwPollEvents();
  }
}
void App::createPipelineLayout() {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};

  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;
  pipelineLayoutInfo.pSetLayouts = nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;
  if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr,
                             &pipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("failed to create pipeline laoyout!");
  }
}
void App::createPipeline() {
  auto pipelineConfig = Pipeline::defaultPipelineConfigInfo(swapChain.width(),
                                                            swapChain.height());
  pipelineConfig.renderPass = swapChain.getRenderPass();
  pipelineConfig.pipelineLayout = pipelineLayout;
  pipeline =
      std::make_unique<Pipeline>(device, "shaders/shader.vert.spv",
                                 "shaders/shader.frag.spv", pipelineConfig);
}
void App::createCommandBuffers() {}
void App::drawFrame() {}
} // namespace mc
