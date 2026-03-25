#include "point_light_system.hpp"
#include "frame_info.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <memory>
#include <stdexcept>

namespace mc {

  PointLightSystem::PointLightSystem(Device               &device,
                                     VkRenderPass          renderPass,
                                     VkDescriptorSetLayout globalSetLayout)
      : device{device} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
  }
  PointLightSystem::~PointLightSystem() {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
  }

  void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {

    // VkPushConstantRange pushConstantRange{};
    // pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    // pushConstantRange.offset = 0;
    // pushConstantRange.size   = sizeof(SimplePushConstantsData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};

    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create pipeline laoyout!");
    }
  }

  void PointLightSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create a pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescription.clear();
    pipelineConfig.bindingDescription.clear();
    pipelineConfig.renderPass     = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline                      = std::make_unique<Pipeline>(device,
                                                               "shaders/point_light.vert.spv",
                                                               "shaders/point_light.frag.spv",
                                                               pipelineConfig);
  }

  void PointLightSystem::update(FrameInfo &frameInfo, GlobalSceneData &sceneData) {
    int i = 0;
    for (auto &[id, obj] : frameInfo.gameObjects) {
      if (!obj.pointLight)
        continue;
      sceneData.pointLights[i].position = glm::vec4(obj.transform.getTranslation(), 1.f);
      sceneData.pointLights[i].color    = obj.pointLight->color;
      i++;
    }
    sceneData.numLights = lightCount = i;
  }

  void PointLightSystem::render(FrameInfo &frameInfo) {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout,
                            0,
                            1,
                            &frameInfo.globalDescriptorSet,
                            0,
                            nullptr);

    vkCmdDraw(frameInfo.commandBuffer, 6, lightCount, 0, 0);
  }
} // namespace mc
