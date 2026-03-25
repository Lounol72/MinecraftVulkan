#include "material.hpp"
#include <stdexcept>
#include "frame_info.hpp"

namespace mc {
  Material::Material(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
      : device{device} {
    createSet1Layout();
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
    createDepthPipeline(renderPass);
  }
  Material::~Material() {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void Material::createSet1Layout() {
    set1Layout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
  }

  void Material::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        globalSetLayout,
        set1Layout->getDescriptorSetLayout()};
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(SimplePushConstantsData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed to create Pipeline layout");
    }
  }

  void Material::createPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
    pipelineConfig.depthStencilInfo.depthCompareOp   = VK_COMPARE_OP_EQUAL;
    pipelineConfig.renderPass                        = renderPass;
    pipelineConfig.pipelineLayout                    = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(device,
                                          "shaders/shader.vert.spv",
                                          "shaders/shader.frag.spv",
                                          pipelineConfig);
  }

  void Material::createDepthPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo pipelineConfig{};
    Pipeline::depthOnlyPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    depthPipeline                 = std::make_unique<Pipeline>(device,
                                                               "shaders/depth.vert.spv",
                                                               "shaders/depth.frag.spv",
                                                               pipelineConfig);
  }

  MaterialInstance::MaterialInstance(Material                    &material,
                                     DescriptorAllocatorGrowable &allocator,
                                     std::shared_ptr<Texture>     albedo,
                                     std::shared_ptr<Texture>     normal,
                                     std::shared_ptr<Texture>     roughness)
      : material{material},
        albedo{albedo},
        normal{normal},
        roughness{roughness} {
    auto albedoInfo    = albedo->descriptorInfo();
    auto normalInfo    = normal->descriptorInfo();
    auto roughnessInfo = roughness->descriptorInfo();

    DescriptorWriter writer(material.getSetLayout());
    writer.writeImage(0, &albedoInfo)
        .writeImage(1, &normalInfo)
        .writeImage(2, &roughnessInfo)
        .build(descriptorSet, allocator);
  }
  void MaterialInstance::bindPipeline(VkCommandBuffer commandBuffer,
                                      VkDescriptorSet globalDescriptorSet) {
    material.bind(commandBuffer);

    std::array<VkDescriptorSet, 2> sets = {globalDescriptorSet, descriptorSet};
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            material.getPipelineLayout(),
                            0,
                            static_cast<uint32_t>(sets.size()),
                            sets.data(),
                            0,
                            nullptr);
  }
  void MaterialInstance::bindDescriptorSet(VkCommandBuffer commandBuffer) {
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            material.getPipelineLayout(),
                            1, // firstSet = 1, on saute set0
                            1, // une seule set
                            &descriptorSet,
                            0,
                            nullptr);
  }
} // namespace mc
