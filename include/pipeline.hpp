#pragma once

#include <sys/types.h>
#include <vulkan/vulkan_core.h>
#include <string>
#include <vector>
#include "device.hpp"

namespace mc {

  // Placeholder for future pipeline configuration (viewport, rasterizer,
  // blend state, etc.). Passed to the constructor to keep it flexible.
  struct PipelineConfigInfo {
    PipelineConfigInfo()                                      = default;
    PipelineConfigInfo(const PipelineConfigInfo &)            = delete;
    PipelineConfigInfo &operator=(const PipelineConfigInfo &) = delete;

    std::vector<VkVertexInputBindingDescription>   bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescription{};
    std::vector<VkDynamicState>                    dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo               dynamicStateInfo;
    VkPipelineInputAssemblyStateCreateInfo         inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo         rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo           multisampleInfo;
    VkPipelineColorBlendAttachmentState            colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo            colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo          depthStencilInfo;
    VkPipelineLayout                               pipelineLayout = nullptr;
    VkRenderPass                                   renderPass     = nullptr;
    uint32_t                                       subpass        = 0;
  };

  // Owns the VkPipeline and its shader modules.
  // Non-copyable: pipeline objects hold GPU resources that must not be aliased.
  class Pipeline {
  public:
    Pipeline(Device                   &device,
             const std::string        &vertFilePath,
             const std::string        &fragFilePath,
             const PipelineConfigInfo &configInfo);

    ~Pipeline();
    Pipeline(const Pipeline &)            = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    // Returns a sensible default config for a full-screen viewport of the given
    // dimensions.
    static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);
    static void depthOnlyPipelineConfigInfo(PipelineConfigInfo &configInfo);

    void bind(VkCommandBuffer commandBuffer) {
      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    }

  private:
    // Reads a binary file (e.g. SPIR-V) fully into a byte buffer.
    static std::vector<char> readFile(const std::string &filePath);

    void createGraphicsPipeline(const std::string        &vertFilePath,
                                const std::string        &fragFilePath,
                                const PipelineConfigInfo &configInfo);

    // Wraps raw SPIR-V bytecode into a VkShaderModule that Vulkan can use.
    void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

    Device        &device;
    VkPipeline     graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
  };
} // namespace mc
