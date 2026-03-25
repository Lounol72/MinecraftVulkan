#include "ibl_precomputer.hpp"

#include <fstream>
#include <stdexcept>
#include <cmath>
#include <stb_image.h>

#include "buffer.hpp"

namespace mc {

  // ─── IblMaps RAII ────────────────────────────────────────────────────────────

  IblPrecomputer::IblMaps::~IblMaps() {
    if (!device_) return;
    VkDevice dev = device_->device();
    if (brdfLutSampler) vkDestroySampler(dev, brdfLutSampler, nullptr);
    if (brdfLutView)    vkDestroyImageView(dev, brdfLutView, nullptr);
    if (brdfLutImage)   vkDestroyImage(dev, brdfLutImage, nullptr);
    if (brdfLutMemory)  vkFreeMemory(dev, brdfLutMemory, nullptr);
  }

  IblPrecomputer::IblMaps::IblMaps(IblMaps &&o) noexcept
      : device_{o.device_},
        irradianceMap{std::move(o.irradianceMap)},
        prefilteredMap{std::move(o.prefilteredMap)},
        brdfLutImage{o.brdfLutImage},
        brdfLutView{o.brdfLutView},
        brdfLutSampler{o.brdfLutSampler},
        brdfLutMemory{o.brdfLutMemory} {
    o.brdfLutImage   = VK_NULL_HANDLE;
    o.brdfLutView    = VK_NULL_HANDLE;
    o.brdfLutSampler = VK_NULL_HANDLE;
    o.brdfLutMemory  = VK_NULL_HANDLE;
    o.device_        = nullptr;
  }

  IblPrecomputer::IblMaps &IblPrecomputer::IblMaps::operator=(IblMaps &&o) noexcept {
    if (this == &o) return *this;
    this->~IblMaps();
    new (this) IblMaps(std::move(o));
    return *this;
  }

  VkDescriptorImageInfo IblPrecomputer::IblMaps::brdfLutDescriptorInfo() const {
    return {brdfLutSampler, brdfLutView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

  // ─── Constructor / Destructor ─────────────────────────────────────────────────

  IblPrecomputer::IblPrecomputer(Device &device) : device{device} {
    // Descriptor pool shared across all compute stages:
    //   7 COMBINED_IMAGE_SAMPLER + 8 STORAGE_IMAGE = 8 sets max
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          8},
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = 8;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
    if (vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &computePool) != VK_SUCCESS)
      throw std::runtime_error("IblPrecomputer: failed to create compute descriptor pool");

    createEquirectToCubeStage();
    createIrradianceStage();
    createPrefilterStage();
    createBrdfLutStage();
  }

  IblPrecomputer::~IblPrecomputer() {
    destroyStage(equirectStage);
    destroyStage(irradianceStage);
    destroyStage(prefilterStage);
    destroyStage(brdfLutStage);
    if (computePool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(device.device(), computePool, nullptr);
  }

  // ─── Stage creation helpers ───────────────────────────────────────────────────

  void IblPrecomputer::createEquirectToCubeStage() {
    equirectStage.setLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

    VkDescriptorSetLayout raw = equirectStage.setLayout->getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &raw;
    vkCreatePipelineLayout(device.device(), &layoutInfo, nullptr, &equirectStage.pipelineLayout);
    equirectStage.pipeline =
        createComputePipeline("shaders/equirect_to_cube.comp.spv", equirectStage.pipelineLayout);
  }

  void IblPrecomputer::createIrradianceStage() {
    irradianceStage.setLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

    VkDescriptorSetLayout raw = irradianceStage.setLayout->getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &raw;
    vkCreatePipelineLayout(device.device(), &layoutInfo, nullptr, &irradianceStage.pipelineLayout);
    irradianceStage.pipeline =
        createComputePipeline("shaders/irradiance.comp.spv", irradianceStage.pipelineLayout);
  }

  void IblPrecomputer::createPrefilterStage() {
    prefilterStage.setLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

    VkDescriptorSetLayout raw = prefilterStage.setLayout->getDescriptorSetLayout();
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(float); // roughness

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &raw;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;
    vkCreatePipelineLayout(device.device(), &layoutInfo, nullptr, &prefilterStage.pipelineLayout);
    prefilterStage.pipeline =
        createComputePipeline("shaders/prefilter.comp.spv", prefilterStage.pipelineLayout);
  }

  void IblPrecomputer::createBrdfLutStage() {
    brdfLutStage.setLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();

    VkDescriptorSetLayout raw = brdfLutStage.setLayout->getDescriptorSetLayout();
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &raw;
    vkCreatePipelineLayout(device.device(), &layoutInfo, nullptr, &brdfLutStage.pipelineLayout);
    brdfLutStage.pipeline =
        createComputePipeline("shaders/brdf_lut.comp.spv", brdfLutStage.pipelineLayout);
  }

  void IblPrecomputer::destroyStage(ComputeStage &stage) {
    VkDevice dev = device.device();
    if (stage.pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(dev, stage.pipeline, nullptr);
    if (stage.pipelineLayout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(dev, stage.pipelineLayout, nullptr);
    stage.setLayout.reset();
  }

  // ─── HDR loading ──────────────────────────────────────────────────────────────

  IblPrecomputer::EquirectImage IblPrecomputer::loadHdr(const std::string &path) {
    int   w, h, channels;
    bool  loadedWithStbi = false;
    float *pixels        = nullptr;

    if (!path.empty()) {
      pixels = stbi_loadf(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
      if (pixels) loadedWithStbi = true;
    }

    if (!pixels) {
      // Fallback: 1×1 white HDR (uniform white environment)
      static float white[4] = {1.f, 1.f, 1.f, 1.f};
      pixels = white;
      w = h  = 1;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

    Buffer stagingBuffer{device, imageSize, 1,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    stagingBuffer.map();
    stagingBuffer.writeToBuffer(pixels);

    if (loadedWithStbi) stbi_image_free(pixels);

    EquirectImage img{};

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img.image, img.memory);

    VkCommandBuffer cmd = device.beginSingleTimeCommands();

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image                           = img.image;
    toTransfer.subresourceRange               = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toTransfer.srcAccessMask                   = 0;
    toTransfer.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent      = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(), img.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toShader = toTransfer;
    toShader.oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toShader);

    device.endSingleTimeCommands(cmd);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = img.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange               = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device.device(), &viewInfo, nullptr, &img.view);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCreateSampler(device.device(), &samplerInfo, nullptr, &img.sampler);

    return img;
  }

  void IblPrecomputer::destroyEquirectImage(EquirectImage &img) {
    VkDevice dev = device.device();
    vkDestroySampler(dev, img.sampler, nullptr);
    vkDestroyImageView(dev, img.view, nullptr);
    vkDestroyImage(dev, img.image, nullptr);
    vkFreeMemory(dev, img.memory, nullptr);
    img = {};
  }

  // ─── Descriptor helpers ───────────────────────────────────────────────────────

  VkDescriptorSet IblPrecomputer::allocateComputeSet(const DescriptorSetLayout &layout) {
    VkDescriptorSetLayout       raw = layout.getDescriptorSetLayout();
    VkDescriptorSetAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = computePool;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &raw;
    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(device.device(), &info, &set) != VK_SUCCESS)
      throw std::runtime_error("IblPrecomputer: failed to allocate compute descriptor set");
    return set;
  }

  void IblPrecomputer::writeSampledImage(VkDescriptorSet set, uint32_t binding,
                                         VkDescriptorImageInfo &info) {
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set;
    write.dstBinding      = binding;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &info;
    vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
  }

  void IblPrecomputer::writeStorageImage(VkDescriptorSet set, uint32_t binding,
                                         VkDescriptorImageInfo &info) {
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = set;
    write.dstBinding      = binding;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo      = &info;
    vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
  }

  // ─── Main precompute ──────────────────────────────────────────────────────────

  IblPrecomputer::IblMaps IblPrecomputer::precompute(const std::string &hdrPath) {
    EquirectImage equirect = loadHdr(hdrPath);

    // ── Base cubemap (equirect → cube) ──────────────────────────────────────────
    auto envCube = std::make_unique<CubeTexture>(device, IBL_CUBEMAP_SIZE, 1,
                                                 VK_FORMAT_R16G16B16A16_SFLOAT);

    VkCommandBuffer cmd = device.beginSingleTimeCommands();

    // Transition envCube to GENERAL for compute write
    envCube->transitionLayout(cmd,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    VkDescriptorSet equirectSet = allocateComputeSet(*equirectStage.setLayout);
    VkDescriptorImageInfo equirectInfo{equirect.sampler, equirect.view,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo cubeOut{VK_NULL_HANDLE, envCube->storageViewForMip(0),
                                  VK_IMAGE_LAYOUT_GENERAL};
    writeSampledImage(equirectSet, 0, equirectInfo);
    writeStorageImage(equirectSet, 1, cubeOut);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, equirectStage.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            equirectStage.pipelineLayout, 0, 1, &equirectSet, 0, nullptr);
    uint32_t groups = (IBL_CUBEMAP_SIZE + 15) / 16;
    vkCmdDispatch(cmd, groups, groups, 6);

    // Transition envCube to SHADER_READ_ONLY for use as input
    envCube->transitionLayout(cmd,
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

    // ── Irradiance map ───────────────────────────────────────────────────────────
    auto irradianceMap = std::make_unique<CubeTexture>(device, IBL_IRRADIANCE_SIZE, 1,
                                                       VK_FORMAT_R16G16B16A16_SFLOAT);

    irradianceMap->transitionLayout(cmd,
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    VkDescriptorSet irrSet = allocateComputeSet(*irradianceStage.setLayout);
    VkDescriptorImageInfo envCubeInfo = envCube->descriptorInfo();
    VkDescriptorImageInfo irrOut{VK_NULL_HANDLE, irradianceMap->storageViewForMip(0),
                                 VK_IMAGE_LAYOUT_GENERAL};
    writeSampledImage(irrSet, 0, envCubeInfo);
    writeStorageImage(irrSet, 1, irrOut);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, irradianceStage.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            irradianceStage.pipelineLayout, 0, 1, &irrSet, 0, nullptr);
    uint32_t irrGroups = (IBL_IRRADIANCE_SIZE + 15) / 16;
    vkCmdDispatch(cmd, irrGroups, irrGroups, 6);

    irradianceMap->transitionLayout(cmd,
                                    VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

    // ── Prefiltered env map ──────────────────────────────────────────────────────
    auto prefilteredMap = std::make_unique<CubeTexture>(device, IBL_PREFILTER_SIZE, IBL_PREFILTER_MIPS,
                                                        VK_FORMAT_R16G16B16A16_SFLOAT);

    prefilteredMap->transitionLayout(cmd,
                                     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    for (uint32_t mip = 0; mip < IBL_PREFILTER_MIPS; ++mip) {
      float    roughness = static_cast<float>(mip) / static_cast<float>(IBL_PREFILTER_MIPS - 1);
      uint32_t mipSize   = IBL_PREFILTER_SIZE >> mip; // 256, 128, 64, 32, 16

      VkDescriptorSet prefSet = allocateComputeSet(*prefilterStage.setLayout);
      VkDescriptorImageInfo prefEnvInfo = envCube->descriptorInfo();
      VkDescriptorImageInfo prefOut{VK_NULL_HANDLE, prefilteredMap->storageViewForMip(mip),
                                    VK_IMAGE_LAYOUT_GENERAL};
      writeSampledImage(prefSet, 0, prefEnvInfo);
      writeStorageImage(prefSet, 1, prefOut);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prefilterStage.pipeline);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                              prefilterStage.pipelineLayout, 0, 1, &prefSet, 0, nullptr);
      vkCmdPushConstants(cmd, prefilterStage.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                         0, sizeof(float), &roughness);
      uint32_t prefGroups = std::max(1u, (mipSize + 15) / 16);
      vkCmdDispatch(cmd, prefGroups, prefGroups, 6);
    }

    prefilteredMap->transitionLayout(cmd,
                                     VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    irradianceMap->transitionLayout(cmd,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

    // ── BRDF LUT ─────────────────────────────────────────────────────────────────
    IblMaps maps;
    maps.device_ = &device;
    maps.irradianceMap  = std::move(irradianceMap);
    maps.prefilteredMap = std::move(prefilteredMap);

    VkImageCreateInfo brdfInfo{};
    brdfInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    brdfInfo.imageType     = VK_IMAGE_TYPE_2D;
    brdfInfo.extent        = {IBL_BRDF_LUT_SIZE, IBL_BRDF_LUT_SIZE, 1};
    brdfInfo.mipLevels     = 1;
    brdfInfo.arrayLayers   = 1;
    brdfInfo.format        = VK_FORMAT_R16G16_SFLOAT;
    brdfInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    brdfInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    brdfInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    brdfInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    brdfInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    device.createImageWithInfo(brdfInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               maps.brdfLutImage, maps.brdfLutMemory);

    VkImageMemoryBarrier brdfToGeneral{};
    brdfToGeneral.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    brdfToGeneral.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    brdfToGeneral.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
    brdfToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    brdfToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    brdfToGeneral.image               = maps.brdfLutImage;
    brdfToGeneral.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    brdfToGeneral.srcAccessMask       = 0;
    brdfToGeneral.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &brdfToGeneral);

    VkImageViewCreateInfo brdfViewInfo{};
    brdfViewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    brdfViewInfo.image            = maps.brdfLutImage;
    brdfViewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    brdfViewInfo.format           = VK_FORMAT_R16G16_SFLOAT;
    brdfViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device.device(), &brdfViewInfo, nullptr, &maps.brdfLutView);

    VkDescriptorSet brdfSet = allocateComputeSet(*brdfLutStage.setLayout);
    VkDescriptorImageInfo brdfStorageInfo{VK_NULL_HANDLE, maps.brdfLutView, VK_IMAGE_LAYOUT_GENERAL};
    writeStorageImage(brdfSet, 0, brdfStorageInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, brdfLutStage.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            brdfLutStage.pipelineLayout, 0, 1, &brdfSet, 0, nullptr);
    uint32_t lutGroups = (IBL_BRDF_LUT_SIZE + 15) / 16;
    vkCmdDispatch(cmd, lutGroups, lutGroups, 1);

    VkImageMemoryBarrier brdfToShader = brdfToGeneral;
    brdfToShader.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    brdfToShader.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brdfToShader.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    brdfToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &brdfToShader);

    device.endSingleTimeCommands(cmd);

    // BRDF LUT sampler
    VkSamplerCreateInfo brdfSamplerInfo{};
    brdfSamplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    brdfSamplerInfo.magFilter    = VK_FILTER_LINEAR;
    brdfSamplerInfo.minFilter    = VK_FILTER_LINEAR;
    brdfSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    brdfSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    brdfSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    brdfSamplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCreateSampler(device.device(), &brdfSamplerInfo, nullptr, &maps.brdfLutSampler);

    destroyEquirectImage(equirect);
    // envCube goes out of scope here (only needed for irradiance/prefilter)

    return maps;
  }

  // ─── Compute pipeline creation ────────────────────────────────────────────────

  VkPipeline IblPrecomputer::createComputePipeline(const std::string &spvPath,
                                                    VkPipelineLayout   layout) {
    auto code = readFile(spvPath);

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device.device(), &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS)
      throw std::runtime_error("IblPrecomputer: failed to create shader module: " + spvPath);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName  = "main";
    pipelineInfo.layout       = layout;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device.device(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                 nullptr, &pipeline) != VK_SUCCESS)
      throw std::runtime_error("IblPrecomputer: failed to create compute pipeline: " + spvPath);

    vkDestroyShaderModule(device.device(), shaderModule, nullptr);
    return pipeline;
  }

  std::vector<char> IblPrecomputer::readFile(const std::string &path) {
    std::ifstream file{path, std::ios::ate | std::ios::binary};
    if (!file.is_open())
      throw std::runtime_error("IblPrecomputer: failed to open file: " + path);
    size_t            size = static_cast<size_t>(file.tellg());
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), size);
    return buf;
  }

} // namespace mc
