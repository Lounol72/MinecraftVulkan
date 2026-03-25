#pragma once

#include <stb_image.h>
#include <vulkan/vulkan_core.h>
#include <string>
#include "device.hpp"

namespace mc {
  class Texture {
  public:
    Texture(Device &device, const std::string &filePath, bool srgb = true);
    Texture(Device &device, int w, int h, const stbi_uc *pixels, bool srgb = true);
    ~Texture();

    Texture(const Texture &)            = delete;
    Texture &operator=(const Texture &) = delete;

    VkDescriptorImageInfo descriptorInfo() const;

  private:
    void createImage(int w, int h, const stbi_uc *pixels, VkFormat format);
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void createImageView();
    void createSampler();

    Device        &device;
    uint32_t       mipLevels    = 1;
    VkFormat       format       = VK_FORMAT_R8G8B8A8_SRGB;
    VkImage        image        = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VkImageView    imageView    = VK_NULL_HANDLE;
    VkSampler      sampler      = VK_NULL_HANDLE;
  };
} // namespace mc
