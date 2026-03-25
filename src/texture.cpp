#include "texture.hpp"
#include <stdexcept>
#include "buffer.hpp"
#include "device.hpp"

namespace mc {
  /**
   * La classe texture permets de créer une image à partir d'un fichier ou d'une hauteur et largeur.
   * Chaque instance appartiendra à un material.
   */

  /*
   * Construit la texture à partir d'un fichier.
   * @param &device : mc::Device
   * @param &filePath : const std::string
   *
   * */
  Texture::Texture(Device &device, const std::string &filePath)
      : device{device} {
    int      w, h, channels;
    stbi_uc *pixels = stbi_load(filePath.c_str(), &w, &h, &channels, STBI_rgb_alpha);

    if (!pixels) {
      throw std::runtime_error("Failed to load texture: " + filePath);
    }

    createImage(w, h, pixels);
    stbi_image_free(pixels);

    createImageView();
    createSampler();
  }

  /*
   * Construit la texture à partir d'une taille désirée w * h. Récupération de pixels
   * @param &device : mc::Device
   * @param w : int
   * @param h : int
   * @param *pixels : stbi_uc
   *
   * */
  Texture::Texture(Device &device, int w, int h, const stbi_uc *pixels)
      : device{device} {
    createImage(w, h, pixels);
    createImageView();
    createSampler();
  }
  /*
   * Desctructeur : D'abord le sampler, puis l'imageView, l'image et enfin le deviceMemory
   *
   * */
  Texture::~Texture() {
    vkDestroySampler(device.device(), sampler, nullptr);
    vkDestroyImageView(device.device(), imageView, nullptr);
    vkDestroyImage(device.device(), image, nullptr);
    vkFreeMemory(device.device(), deviceMemory, nullptr);
  }

  /*
   * Transmets au GPU l'image à partir des différents paramètres.
   * @param w : int
   * @param h : int
   * @param *pixels : stbi_uc
   *
   * */
  void Texture::createImage(int w, int h, const stbi_uc *pixels) {
    VkDeviceSize imageSize = w * h * 4; // RGBA

    // StagingBuffer
    Buffer stagingBuffer{device,
                         imageSize,
                         1,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};

    stagingBuffer.map();
    stagingBuffer.writeToBuffer(pixels);

    // Création de des informations de l'image.
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = static_cast<uint32_t>(w);
    imageInfo.extent.height = static_cast<uint32_t>(h);
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);

    // Transitions + copie en un seul command buffer
    VkCommandBuffer cmd = device.beginSingleTimeCommands();

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image                           = image;
    toTransfer.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel   = 0;
    toTransfer.subresourceRange.levelCount     = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount     = 1;
    toTransfer.srcAccessMask                   = 0;
    toTransfer.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &toTransfer);

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    vkCmdCopyBufferToImage(cmd,
                           stagingBuffer.getBuffer(),
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    VkImageMemoryBarrier toShader{};
    toShader.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShader.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toShader.image                           = image;
    toShader.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    toShader.subresourceRange.baseMipLevel   = 0;
    toShader.subresourceRange.levelCount     = 1;
    toShader.subresourceRange.baseArrayLayer = 0;
    toShader.subresourceRange.layerCount     = 1;
    toShader.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &toShader);

    device.endSingleTimeCommands(cmd);
  }
  void Texture::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cmd = device.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      srcStage              = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      dstStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      srcStage              = VK_PIPELINE_STAGE_TRANSFER_BIT;
      dstStage              = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
      throw std::invalid_argument("Unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    device.endSingleTimeCommands(cmd);
  }

  void Texture::createImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
      throw std::runtime_error("Failed to create texture image view");
  }

  void Texture::createSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter               = VK_FILTER_LINEAR;
    samplerInfo.minFilter               = VK_FILTER_LINEAR;
    samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable        = VK_TRUE;
    samplerInfo.maxAnisotropy           = device.properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
      throw std::runtime_error("Failed to create texture sampler");
  }

  VkDescriptorImageInfo Texture::descriptorInfo() const {
    return VkDescriptorImageInfo{sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }
} // namespace mc
