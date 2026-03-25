#include "cube_texture.hpp"
#include <stdexcept>

namespace mc {

  CubeTexture::CubeTexture(Device &device, uint32_t size, uint32_t mipLevels, VkFormat format)
      : device{device}, size{size}, mipLevels{mipLevels}, format{format} {
    createImage();
    createSamplerView();
    createStorageViews();
    createSampler();
  }

  CubeTexture::~CubeTexture() {
    VkDevice dev = device.device();
    vkDestroySampler(dev, sampler, nullptr);
    vkDestroyImageView(dev, samplerView, nullptr);
    for (VkImageView view : storageViews)
      vkDestroyImageView(dev, view, nullptr);
    vkDestroyImage(dev, image, nullptr);
    vkFreeMemory(dev, deviceMemory, nullptr);
  }

  // Creates a cube-compatible VkImage with 6 array layers and the requested mip levels.
  // Usage flags include STORAGE (compute write), SAMPLED (shader read), and TRANSFER_DST.
  void CubeTexture::createImage() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = size;
    imageInfo.extent.height = size;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 6;
    imageInfo.format        = format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);
  }

  // Cube view spanning all 6 faces and all mip levels — used for sampler binding in shaders.
  void CubeTexture::createSamplerView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;

    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &samplerView) != VK_SUCCESS)
      throw std::runtime_error("CubeTexture: failed to create sampler view");
  }

  // One 2D_ARRAY view per mip level, each covering all 6 faces.
  // These are used as storage image targets in compute shaders (imageStore).
  void CubeTexture::createStorageViews() {
    storageViews.resize(mipLevels);

    for (uint32_t mip = 0; mip < mipLevels; ++mip) {
      VkImageViewCreateInfo viewInfo{};
      viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image                           = image;
      viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      viewInfo.format                          = format;
      viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      viewInfo.subresourceRange.baseMipLevel   = mip;
      viewInfo.subresourceRange.levelCount     = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount     = 6;

      if (vkCreateImageView(device.device(), &viewInfo, nullptr, &storageViews[mip]) != VK_SUCCESS)
        throw std::runtime_error("CubeTexture: failed to create storage view for mip " +
                                 std::to_string(mip));
    }
  }

  // LINEAR filtering, CLAMP_TO_EDGE on all axes, LINEAR mipmap mode.
  // maxLod is set to the full mip chain so all levels are reachable.
  void CubeTexture::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = device.properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = static_cast<float>(mipLevels);

    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
      throw std::runtime_error("CubeTexture: failed to create sampler");
  }

  // Inserts a pipeline barrier covering all 6 array layers for the given mip range.
  // baseMip + mipCount default to covering the entire chain (VK_REMAINING_MIP_LEVELS).
  void CubeTexture::transitionLayout(VkCommandBuffer      cmd,
                                     VkImageLayout        oldLayout,
                                     VkImageLayout        newLayout,
                                     VkPipelineStageFlags srcStage,
                                     VkAccessFlags        srcAccess,
                                     VkPipelineStageFlags dstStage,
                                     VkAccessFlags        dstAccess,
                                     uint32_t             baseMip,
                                     uint32_t             mipCount) {
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = baseMip;
    barrier.subresourceRange.levelCount     = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 6;
    barrier.srcAccessMask                   = srcAccess;
    barrier.dstAccessMask                   = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
  }

  VkDescriptorImageInfo CubeTexture::descriptorInfo() const {
    return VkDescriptorImageInfo{sampler, samplerView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

} // namespace mc
