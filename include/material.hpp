#pragma once

#include <vulkan/vulkan_core.h>
#include "descriptors.hpp"
#include "device.hpp"
#include "pipeline.hpp"
#include "texture.hpp"

namespace mc {
  class Material {
  public:
    Material(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~Material();

    Material(const Material &)            = delete;
    Material &operator=(const Material &) = delete;

    void bind(VkCommandBuffer commandBuffer)           { pipeline->bind(commandBuffer); }
    void bindDepthPipeline(VkCommandBuffer commandBuffer) const { depthPipeline->bind(commandBuffer); }

    VkPipelineLayout getPipelineLayout() const {
      return pipelineLayout;
    }
    DescriptorSetLayout &getSetLayout() {
      return *set1Layout;
    }

  private:
    void createSet1Layout();
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createDepthPipeline(VkRenderPass renderPass);

    Device &device;

    std::unique_ptr<DescriptorSetLayout> set1Layout;
    VkPipelineLayout                     pipelineLayout = VK_NULL_HANDLE;
    std::unique_ptr<Pipeline>            pipeline;
    std::unique_ptr<Pipeline>            depthPipeline;
  };

  class MaterialInstance {
  public:
    MaterialInstance(Material                    &material,
                     DescriptorAllocatorGrowable &allocator,
                     std::shared_ptr<Texture>     albedo,
                     std::shared_ptr<Texture>     normal,
                     std::shared_ptr<Texture>     roughness);

    MaterialInstance(const MaterialInstance &)            = delete;
    MaterialInstance &operator=(const MaterialInstance &) = delete;

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
    std::shared_ptr<Texture> roughness;
  };
} // namespace mc
