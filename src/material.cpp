#include "material.hpp"
#include <stdexcept>
#include "frame_info.hpp"

namespace mc {
  Material::Material(Device               &device,
                     VkRenderPass          renderPass,
                     VkDescriptorSetLayout globalSetLayout,
                     VkDescriptorSetLayout iblSetLayout,
                     ToneMapMode           toneMapMode)
      : device{device},
        toneMapMode{toneMapMode} {
    createSet1Layout();
    createPipelineLayout(globalSetLayout, iblSetLayout);
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
            .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_FRAGMENT_BIT)
            .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build();
  }

  void Material::createPipelineLayout(VkDescriptorSetLayout globalSetLayout,
                                       VkDescriptorSetLayout iblSetLayout) {
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
        globalSetLayout,
        set1Layout->getDescriptorSetLayout(),
        iblSetLayout};
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
    VkSpecializationMapEntry mapEntry{};
    mapEntry.constantID = 0;
    mapEntry.offset     = 0;
    mapEntry.size       = sizeof(uint32_t);

    uint32_t toneMapModeValue = static_cast<uint32_t>(toneMapMode);

    // toneMapModeValue est sur la stack — valide pour toute la durée de l'appel
    // vkCreateGraphicsPipelines (appelé dans Pipeline()) ne retient pas ce pointeur.
    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = 1;
    specInfo.pMapEntries   = &mapEntry;
    specInfo.dataSize      = sizeof(uint32_t);
    specInfo.pData         = &toneMapModeValue;

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
    pipelineConfig.depthStencilInfo.depthCompareOp   = VK_COMPARE_OP_LESS;
    pipelineConfig.renderPass                        = renderPass;
    pipelineConfig.pipelineLayout                    = pipelineLayout;
    pipelineConfig.fragSpecializationInfo            = &specInfo;
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

  MaterialInstance::MaterialInstance(Device                      &device,
                                     Material                    &material,
                                     DescriptorAllocatorGrowable &allocator,
                                     std::shared_ptr<Texture>     albedo,
                                     std::shared_ptr<Texture>     normal,
                                     std::shared_ptr<Texture>     orm,
                                     std::shared_ptr<Texture>     emissive)
      : material{material},
        albedo{albedo},
        normal{normal},
        orm{orm},
        emissive{emissive} {
    paramBuffer = std::make_unique<Buffer>(
        device,
        sizeof(MaterialParamsUBO),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    paramBuffer->map();

    MaterialParamsUBO defaultParams{};
    paramBuffer->writeToBuffer(&defaultParams);

    auto albedoInfo   = albedo->descriptorInfo();
    auto normalInfo   = normal->descriptorInfo();
    auto ormInfo      = orm->descriptorInfo();
    auto paramsInfo   = paramBuffer->descriptorInfo();
    auto emissiveInfo = emissive->descriptorInfo();

    DescriptorWriter writer(material.getSetLayout());
    writer.writeImage(0, &albedoInfo)
        .writeImage(1, &normalInfo)
        .writeImage(2, &ormInfo)
        .writeBuffer(3, &paramsInfo)
        .writeImage(4, &emissiveInfo)
        .build(descriptorSet, allocator);
  }

  void MaterialInstance::setParams(const MaterialParamsUBO &params) {
    paramBuffer->writeToBuffer(&params);
    paramBuffer->flush();
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
