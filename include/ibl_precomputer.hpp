#pragma once

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "cube_texture.hpp"
#include "descriptors.hpp"
#include "device.hpp"

namespace mc {

  static constexpr uint32_t IBL_CUBEMAP_SIZE    = 512;
  static constexpr uint32_t IBL_IRRADIANCE_SIZE = 32;
  static constexpr uint32_t IBL_PREFILTER_SIZE  = 256;
  static constexpr uint32_t IBL_PREFILTER_MIPS  = 5;
  static constexpr uint32_t IBL_BRDF_LUT_SIZE   = 512;

  // Runs all IBL precomputation at startup via compute shaders.
  // Produces irradiance map, prefiltered env map, and BRDF LUT from an equirectangular HDR file.
  class IblPrecomputer {
  public:
    struct IblMaps {
      IblMaps() = default;
      ~IblMaps();
      IblMaps(IblMaps &&) noexcept;
      IblMaps &operator=(IblMaps &&) noexcept;
      IblMaps(const IblMaps &)            = delete;
      IblMaps &operator=(const IblMaps &) = delete;

      Device                      *device_       = nullptr;
      std::unique_ptr<CubeTexture> irradianceMap;   // 32×32
      std::unique_ptr<CubeTexture> prefilteredMap;  // 256×256, IBL_PREFILTER_MIPS mips
      VkImage                      brdfLutImage   = VK_NULL_HANDLE;
      VkImageView                  brdfLutView    = VK_NULL_HANDLE;
      VkSampler                    brdfLutSampler = VK_NULL_HANDLE;
      VkDeviceMemory               brdfLutMemory  = VK_NULL_HANDLE;

      VkDescriptorImageInfo brdfLutDescriptorInfo() const;
    };

    IblPrecomputer(Device &device);
    ~IblPrecomputer();

    IblPrecomputer(const IblPrecomputer &)            = delete;
    IblPrecomputer &operator=(const IblPrecomputer &) = delete;

    // Loads the HDR file and runs all compute passes.
    // If hdrPath is empty or the file doesn't exist, a uniform white environment is used.
    IblMaps precompute(const std::string &hdrPath);

  private:
    struct ComputeStage {
      std::unique_ptr<DescriptorSetLayout> setLayout;
      VkPipelineLayout                     pipelineLayout = VK_NULL_HANDLE;
      VkPipeline                           pipeline       = VK_NULL_HANDLE;
    };

    // Temporary equirect 2D HDR texture — uploaded once, discarded after equirect_to_cube.
    struct EquirectImage {
      VkImage        image   = VK_NULL_HANDLE;
      VkDeviceMemory memory  = VK_NULL_HANDLE;
      VkImageView    view    = VK_NULL_HANDLE;
      VkSampler      sampler = VK_NULL_HANDLE;
    };

    void createEquirectToCubeStage();
    void createIrradianceStage();
    void createPrefilterStage();
    void createBrdfLutStage();
    void destroyStage(ComputeStage &stage);

    EquirectImage loadHdr(const std::string &path);
    void          destroyEquirectImage(EquirectImage &img);

    // Allocate + write a compute descriptor set from the stage's set layout.
    VkDescriptorSet allocateComputeSet(const DescriptorSetLayout &layout);
    void            writeSampledImage(VkDescriptorSet set, uint32_t binding,
                                      VkDescriptorImageInfo &info);
    void            writeStorageImage(VkDescriptorSet set, uint32_t binding,
                                      VkDescriptorImageInfo &info);

    VkPipeline               createComputePipeline(const std::string &spvPath,
                                                   VkPipelineLayout   layout);
    static std::vector<char> readFile(const std::string &path);

    Device &device;

    VkDescriptorPool computePool = VK_NULL_HANDLE;

    ComputeStage equirectStage;
    ComputeStage irradianceStage;
    ComputeStage prefilterStage;
    ComputeStage brdfLutStage;
  };

} // namespace mc
