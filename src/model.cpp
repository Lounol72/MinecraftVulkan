#include "../include/model.hpp"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <vulkan/vulkan_core.h>

namespace mc {

Model::Model(Device &inDevice, const Model::Builder &builder)
    : device{inDevice} {
  createVertexBuffers(builder.vertices);
  createIndexBuffers(builder.indices);
}
Model::~Model() {
  vkDestroyBuffer(device.device(), vertexBuffer, nullptr);
  vkFreeMemory(device.device(), vertexBufferMemory, nullptr);
  if (hasIndexBuffer) {
    vkDestroyBuffer(device.device(), indexBuffer, nullptr);
    vkFreeMemory(device.device(), indexBufferMemory, nullptr);
  }
}

void Model::createVertexBuffers(const std::vector<Vertex> &vertices) {
  vertexCount = static_cast<uint32_t>(vertices.size());
  assert(vertexCount >= 3 && "Vertex count must be at least 3");
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);
  void *data;
  vkMapMemory(device.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
  vkUnmapMemory(device.device(), stagingBufferMemory);

  device.createBuffer(
      bufferSize,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

  device.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

  vkDestroyBuffer(device.device(), stagingBuffer, nullptr);
  vkFreeMemory(device.device(), stagingBufferMemory, nullptr);
}

void Model::createIndexBuffers(const std::vector<uint32_t> &indices) {
  indexCount = static_cast<uint32_t>(indices.size());
  hasIndexBuffer = indexCount > 0;

  if (!hasIndexBuffer)
    return;

  // Choix du type d'indice selon le nombre de vertices :
  // - uint16 (2 bytes) suffit jusqu'à 65 535 vertices -> économise 50% de
  // mémoire GPU
  // - uint32 (4 bytes) nécessaire au-delà (terrains, gros meshes...)
  indexType = indexCount > 65535 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;

  // Conversion uint32 -> uint16 si applicable.
  std::vector<uint16_t> indices16;
  const void *indexData;
  VkDeviceSize bufferSize;

  if (indexType == VK_INDEX_TYPE_UINT16) {
    indices16.assign(indices.begin(), indices.end());
    indexData = indices16.data();
    bufferSize = sizeof(uint16_t) * indexCount;
  } else {
    // Pas de conversion : les données sont déjà en uint32, copie directe.
    indexData = indices.data();
    bufferSize = sizeof(uint32_t) * indexCount;
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);
  void *data;
  vkMapMemory(device.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, indexData, static_cast<size_t>(bufferSize));
  vkUnmapMemory(device.device(), stagingBufferMemory);

  // Création du buffer final en DEVICE_LOCAL (VRAM) et transfert depuis le
  // staging buffer.
  device.createBuffer(
      bufferSize,
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

  device.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

  vkDestroyBuffer(device.device(), stagingBuffer, nullptr);
  vkFreeMemory(device.device(), stagingBufferMemory, nullptr);
}

void Model::draw(VkCommandBuffer commandBuffer) {
  if (hasIndexBuffer) {
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
  } else {
    vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
  }
}

void Model::bind(VkCommandBuffer commandBuffer) {
  VkBuffer buffers[] = {vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

  if (hasIndexBuffer) {
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, indexType);
  }
}

std::vector<VkVertexInputBindingDescription>
Model::Vertex::getBindingDescriptions() {
  std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
  bindingDescriptions[0].binding = 0;
  bindingDescriptions[0].stride = sizeof(Vertex);
  bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription>
Model::Vertex::getAttributeDescriptions() {
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, position);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);
  return attributeDescriptions;
}

} // namespace mc
