#pragma once

#include "device.hpp"
#include <string>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mc {

// Placeholder for future pipeline configuration (viewport, rasterizer,
// blend state, etc.). Passed to the constructor to keep it flexible.
struct PipelineConfigInfo {
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
  VkPipelineRasterizationStateCreateInfo rasterizationInfo;
  VkPipelineMultisampleStateCreateInfo multisampleInfo;
  VkPipelineColorBlendAttachmentState colorBlendAttachment;
  VkPipelineColorBlendStateCreateInfo colorBlendInfo;
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
  VkPipelineLayout pipelineLayout = nullptr;
  VkRenderPass renderPass = nullptr;
  uint32_t subpass = 0;
};

// Owns the VkPipeline and its shader modules.
// Non-copyable: pipeline objects hold GPU resources that must not be aliased.
class Pipeline {
public:
  Pipeline(Device &device, const std::string &vertFilePath,
           const std::string &fragFilePath,
           const PipelineConfigInfo &configInfo);

  ~Pipeline();
  Pipeline(const Pipeline &) = delete;
  void operator=(const Pipeline &) = delete;

  // Returns a sensible default config for a full-screen viewport of the given
  // dimensions.
  static PipelineConfigInfo defaultPipelineConfigInfo(u_int32_t width,
                                                      u_int32_t height);

private:
  // Reads a binary file (e.g. SPIR-V) fully into a byte buffer.
  static std::vector<char> readFile(const std::string &filePath);

  void createGraphicsPipeline(const std::string &vertFilePath,
                              const std::string &fragFilePath,
                              const PipelineConfigInfo &configInfo);

  // Wraps raw SPIR-V bytecode into a VkShaderModule that Vulkan can use.
  void createShaderModule(const std::vector<char> &code,
                          VkShaderModule *shaderModule);

  Device &device;
  VkPipeline graphicsPipeline;
  VkShaderModule vertShaderModule;
  VkShaderModule fragShaderModule;
};
} // namespace mc
