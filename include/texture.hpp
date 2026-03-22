#pragma once

#include <stb_image.h>
#include <vulkan/vulkan_core.h>
#include <string>
#include "device.hpp"

namespace mc {
  class Texture {
  public:
    Texture(Device &device, const std::string &filePath);
    Texture(Device &device, int w, int h, stbi_uc *pixels);
    ~Texture();

    Texture(const Texture &)            = delete;
    Texture &operator=(const Texture &) = delete;

    VkDescriptorImageInfo descriptorInfo() const;

  private:
    void createImage(int w, int h, stbi_uc *pixels);
    void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    void createImageView();
    void createSampler();

    Device        &device;
    VkImage        image        = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VkImageView    imageView    = VK_NULL_HANDLE;
    VkSampler      sampler      = VK_NULL_HANDLE;
  };
} // namespace mc
