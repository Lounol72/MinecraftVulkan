#include "../include/pipeline.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace mc {

Pipeline::Pipeline(Device &device, const std::string &vertFilePath,
                   const std::string &fragFilePath,
                   const PipelineConfigInfo &configInfo)
    : device{device} {
  createGraphicsPipeline(vertFilePath, fragFilePath, configInfo);
}

std::vector<char> Pipeline::readFile(const std::string &filePath) {
  // ios::ate starts at the end so tellg() gives the file size without a second seek.
  std::ifstream file{filePath, std::ios::ate | std::ios::binary};
  if (!file.is_open()) {
    throw std::runtime_error("failed to open file:" + filePath);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();
  return buffer;
}

void Pipeline::createGraphicsPipeline(const std::string &vertFilePath,
                                      const std::string &fragFilePath,
                                      const PipelineConfigInfo &configInfo) {
  auto vertCode = readFile(vertFilePath);
  auto fragCode = readFile(fragFilePath);

  // TODO: create VkPipeline from shader modules and configInfo.
  std::cout << "Vertex Shader Code Size:" << vertCode.size() << std::endl;
  std::cout << "Fragment Shader Code Size:" << fragCode.size() << std::endl;
}
void Pipeline::createShaderModule(const std::vector<char> &code,
                                  VkShaderModule *shaderModule) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  // Vulkan expects uint32_t-aligned SPIR-V; the vector's allocator guarantees this alignment.
  createInfo.pCode = reinterpret_cast<const u_int32_t *>(code.data());

  if (vkCreateShaderModule(device.device(), &createInfo, nullptr,
                           shaderModule) != VK_SUCCESS) {
    throw std::runtime_error("failed to create shader module");
  }
}
PipelineConfigInfo Pipeline::defaultPipelineConfigInfo(u_int32_t width,
                                                       u_int32_t height) {
  PipelineConfigInfo configInfo{};
  return configInfo;
}
} // namespace mc
