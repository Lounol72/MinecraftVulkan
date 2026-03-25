#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "device.hpp"

namespace mc {

  // Cubemap texture with optional mip levels. Supports two views:
  //   - samplerView (VK_IMAGE_VIEW_TYPE_CUBE) for fragment shader sampling
  //   - storageViews[mip] (VK_IMAGE_VIEW_TYPE_2D_ARRAY) for compute shader writes
  class CubeTexture {
  public:
    CubeTexture(Device &device, uint32_t size, uint32_t mipLevels, VkFormat format);
    ~CubeTexture();

    CubeTexture(const CubeTexture &)            = delete;
    CubeTexture &operator=(const CubeTexture &) = delete;

    VkDescriptorImageInfo descriptorInfo() const;
    VkImageView           storageViewForMip(uint32_t mip) const { return storageViews[mip]; }

    void transitionLayout(VkCommandBuffer      cmd,
                          VkImageLayout        oldLayout,
                          VkImageLayout        newLayout,
                          VkPipelineStageFlags srcStage,
                          VkAccessFlags        srcAccess,
                          VkPipelineStageFlags dstStage,
                          VkAccessFlags        dstAccess,
                          uint32_t             baseMip  = 0,
                          uint32_t             mipCount = VK_REMAINING_MIP_LEVELS);

    uint32_t getMipLevels() const { return mipLevels; }
    uint32_t getSize() const      { return size; }
    VkImage  getImage() const     { return image; }
    VkFormat getFormat() const    { return format; }

  private:
    void createImage();
    void createSamplerView();
    void createStorageViews();
    void createSampler();

    Device        &device;
    uint32_t       size;
    uint32_t       mipLevels;
    VkFormat       format;

    VkImage        image        = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VkImageView    samplerView  = VK_NULL_HANDLE;
    std::vector<VkImageView> storageViews; // one per mip, 2D_ARRAY with 6 layers
    VkSampler      sampler      = VK_NULL_HANDLE;
  };

} // namespace mc
