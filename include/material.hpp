#pragma once

#include <vulkan/vulkan_core.h>
#include "buffer.hpp"
#include "descriptors.hpp"
#include "device.hpp"
#include "glm/glm.hpp"
#include "pipeline.hpp"
#include "texture.hpp"

namespace mc {

  enum class ToneMapMode : u_int32_t {
    ACES     = 0,
    Reinhard = 1,
    Khronos  = 2,
  };

  struct MaterialParamsUBO {
    glm::vec4 baseColorFactor{1.f, 1.f, 1.f, 1.f};
    glm::vec4 pbrFactors{1.f, 1.f, 1.f, 1.f};
    glm::vec3 emissiveFactor{0.f};
    float     _pad{0.f};
  };

  class Material {
  public:
    Material(Device               &device,
             VkRenderPass          renderPass,
             VkDescriptorSetLayout globalSetLayout,
             VkDescriptorSetLayout iblSetLayout,
             ToneMapMode           toneMapMode = ToneMapMode::ACES);
    ~Material();

    Material(const Material &)            = delete;
    Material &operator=(const Material &) = delete;

    void bind(VkCommandBuffer commandBuffer) {
      pipeline->bind(commandBuffer);
    }
    void bindDepthPipeline(VkCommandBuffer commandBuffer) const {
      depthPipeline->bind(commandBuffer);
    }

    VkPipelineLayout getPipelineLayout() const {
      return pipelineLayout;
    }
    DescriptorSetLayout &getSetLayout() {
      return *set1Layout;
    }

  private:
    void createSet1Layout();
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout iblSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createDepthPipeline(VkRenderPass renderPass);

    Device     &device;
    ToneMapMode toneMapMode;

    std::unique_ptr<DescriptorSetLayout> set1Layout;
    VkPipelineLayout                     pipelineLayout = VK_NULL_HANDLE;
    std::unique_ptr<Pipeline>            pipeline;
    std::unique_ptr<Pipeline>            depthPipeline;
  };

  class MaterialInstance {
  public:
    MaterialInstance(Device                      &device,
                     Material                    &material,
                     DescriptorAllocatorGrowable &allocator,
                     std::shared_ptr<Texture>     albedo,
                     std::shared_ptr<Texture>     normal,
                     std::shared_ptr<Texture>     orm,
                     std::shared_ptr<Texture>     emissive);

    MaterialInstance(const MaterialInstance &)            = delete;
    MaterialInstance &operator=(const MaterialInstance &) = delete;

    void setParams(const MaterialParamsUBO &params);

    void bindPipeline(VkCommandBuffer commandBuffer, VkDescriptorSet globalDescriptorSet);
    void bindDescriptorSet(VkCommandBuffer commandBuffer);

    VkPipelineLayout getPipelineLayout() const {
      return material.getPipelineLayout();
    }
    Material *getMaterial() const {
      return &material;
    }

  private:
    Material                &material;
    VkDescriptorSet          descriptorSet = VK_NULL_HANDLE;
    std::shared_ptr<Texture> albedo;
    std::shared_ptr<Texture> normal;
    std::shared_ptr<Texture> orm;
    std::shared_ptr<Texture> emissive;
    std::unique_ptr<Buffer>  paramBuffer;
  };
} // namespace mc
