#include "model.hpp"
#include <vulkan/vulkan_core.h>
#include <cstddef>
#include <cstdint>
#include <glm/common.hpp>
#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <vector>
#include "utils.hpp"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

// std
#include <cassert>
#include <cstring>
#include <unordered_map>

namespace std {
  template <>
  struct hash<mc::Model::Vertex> {
    size_t operator()(mc::Model::Vertex const &vertex) const {
      size_t seed = 0;
      mc::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
      return seed;
    }
  };
} // namespace std

namespace {
  template <typename T>
  std::vector<T> readAccessor(const tinygltf::Model &model, int accessorId) {
    const auto    &acc    = model.accessors[accessorId];
    const auto    &bv     = model.bufferViews[acc.bufferView];
    const auto    &buf    = model.buffers[bv.buffer];
    size_t         stride = bv.byteStride ? bv.byteStride : sizeof(T);
    const uint8_t *base   = buf.data.data() + bv.byteOffset + acc.byteOffset;

    std::vector<T> result(acc.count);
    for (size_t i = 0; i < acc.count; i++) {
      memcpy(&result[i], base + i * stride, sizeof(T));
    }
    return result;
  };
} // namespace

namespace mc {

  Model::Model(Device &inDevice, const Model::Builder &builder)
      : device{inDevice},
        albedoIndex{builder.albedoImageIndex},
        normalIndex{builder.normalImageIndex},
        roughnessIndex{builder.roughnessImageIndex} {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
    for (auto &img : builder.images) {
      textures.push_back(
          std::make_shared<Texture>(device, img.width, img.height, img.pixels.data()));
    }
    for (const auto &v : builder.vertices) {
      aabb.min = glm::min(aabb.min, v.position);
      aabb.max = glm::max(aabb.max, v.position);
    }
  }
  Model::~Model() {
  }

  std::unique_ptr<Model> Model::createModelFromFile(Device &device, const std::string &filePath) {
    Builder builder{};
    builder.loadModel(filePath);
    return std::make_unique<Model>(device, builder);
  }

  void Model::createVertexBuffers(const std::vector<Vertex> &vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
    uint32_t     vertexSize = sizeof(vertices[0]);

    Buffer stagingBuffer{
        device,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)vertices.data());

    vertexBuffer = std::make_unique<Buffer>(device,
                                            vertexSize,
                                            vertexCount,
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
  }

  void Model::createIndexBuffers(const std::vector<uint32_t> &indices) {
    indexCount     = static_cast<uint32_t>(indices.size());
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
    const void           *indexData;
    VkDeviceSize          bufferSize;

    if (indexType == VK_INDEX_TYPE_UINT16) {
      indices16.assign(indices.begin(), indices.end());
      indexData  = indices16.data();
      bufferSize = sizeof(uint16_t) * indexCount;
    } else {
      // Pas de conversion : les données sont déjà en uint32, copie directe.
      indexData  = indices.data();
      bufferSize = sizeof(uint32_t) * indexCount;
    }

    uint32_t indexSize = (indexType == VK_INDEX_TYPE_UINT16) ? sizeof(uint16_t) : sizeof(uint32_t);

    Buffer stagingBuffer{
        device,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)indexData);

    indexBuffer = std::make_unique<Buffer>(device,
                                           indexSize,
                                           indexCount,
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
  }

  void Model::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
      vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
      vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
  }

  void Model::bind(VkCommandBuffer commandBuffer) {
    VkBuffer     buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer) {
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, indexType);
    }
  }

  std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding   = 0;
    bindingDescriptions[0].stride    = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
  }

  std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});

    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});

    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});

    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT,        offsetof(Vertex, uv)});
    attributeDescriptions.push_back({4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)});

    return attributeDescriptions;
  }

  void Model::Builder::loadModel(const std::string &filePath) {
    auto endsWith = [](const std::string &s, const std::string &suffix) {
      return s.size() >= suffix.size() &&
             s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (endsWith(filePath, ".glb") || endsWith(filePath, ".gltf")) {
      loadGltf(filePath);
    } else {
      loadObj(filePath);
    }
  }

  void Model::Builder::loadObj(const std::string &filePath) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
      throw std::runtime_error(warn + err);
    }

    vertices.clear();
    indices.clear();

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    for (const auto &shape : shapes) {
      for (const auto &index : shape.mesh.indices) {
        Vertex vertex{};

        if (index.vertex_index >= 0) {
          vertex.position = {attrib.vertices[3 * index.vertex_index + 0],
                             attrib.vertices[3 * index.vertex_index + 1],
                             attrib.vertices[3 * index.vertex_index + 2]};

          auto colorIndex = 3 * index.vertex_index;
          if (colorIndex + 2 < attrib.colors.size()) {
            vertex.color = {attrib.colors[colorIndex + 0],
                            attrib.colors[colorIndex + 1],
                            attrib.colors[colorIndex + 2]};
          } else {
            vertex.color = {1.f, 1.f, 1.f};
          }
        }

        if (index.normal_index >= 0) {
          vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                           attrib.normals[3 * index.normal_index + 1],
                           attrib.normals[3 * index.normal_index + 2]};
        }

        if (index.texcoord_index >= 0) {
          vertex.uv = {attrib.texcoords[2 * index.texcoord_index + 0],
                       attrib.texcoords[2 * index.texcoord_index + 1]};
        }

        if (uniqueVertices.count(vertex) == 0) {
          uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
          vertices.push_back(vertex);
        }
        indices.push_back(uniqueVertices[vertex]);
      }
    }
  }

  void Model::Builder::loadGltf(const std::string &filePath) {
    tinygltf::Model    gltfModel;
    tinygltf::TinyGLTF loader;
    std::string        warn, err;

    std::regex self_regex(".glb", std::regex_constants::ECMAScript | std::regex_constants::icase);

    bool ok = std::regex_search(filePath, self_regex)
                  ? loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath)
                  : loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);
    if (!ok) {
      throw std::runtime_error(warn + err);
    }

    vertices.clear();
    indices.clear();

    for (auto &mesh : gltfModel.meshes) {
      for (auto &primitive : mesh.primitives) {
        uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec4> tangents;

        auto &attrs = primitive.attributes;
        if (attrs.count("POSITION"))
          positions = readAccessor<glm::vec3>(gltfModel, attrs.at("POSITION"));
        if (attrs.count("NORMAL"))
          normals = readAccessor<glm::vec3>(gltfModel, attrs.at("NORMAL"));
        if (attrs.count("TEXCOORD_0"))
          uvs = readAccessor<glm::vec2>(gltfModel, attrs.at("TEXCOORD_0"));
        if (attrs.count("TANGENT"))
          tangents = readAccessor<glm::vec4>(gltfModel, attrs.at("TANGENT"));

        size_t vertexCount = positions.size();
        for (size_t i = 0; i < vertexCount; i++) {
          Vertex v{};
          v.position = positions[i];
          if (!normals.empty())
            v.normal = normals[i];
          if (!uvs.empty())
            v.uv = uvs[i];
          if (!tangents.empty())
            v.tangent = tangents[i];
          v.color = {1.f, 1.f, 1.f};
          vertices.push_back(v);
        }

        if (primitive.indices >= 0) {
          auto          &acc  = gltfModel.accessors[primitive.indices];
          auto          &bv   = gltfModel.bufferViews[acc.bufferView];
          auto          &buf  = gltfModel.buffers[bv.buffer];
          const uint8_t *data = buf.data.data() + bv.byteOffset + acc.byteOffset;

          for (size_t i = 0; i < acc.count; i++) {
            uint32_t idx;
            switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
              idx = reinterpret_cast<const uint8_t *>(data)[i];
              break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
              idx = reinterpret_cast<const uint16_t *>(data)[i];
              break;
            default:
              idx = reinterpret_cast<const uint32_t *>(data)[i];
            }
            indices.push_back(baseVertex + idx);
          }
        }
      }
    }
    if (!gltfModel.materials.empty()) {
      const auto &mat = gltfModel.materials[0];
      const auto &pbr = mat.pbrMetallicRoughness;

      auto resolveSource = [&](int texIndex) -> int {
        if (texIndex < 0 || texIndex >= (int)gltfModel.textures.size())
          return -1;
        return gltfModel.textures[texIndex].source;
      };

      albedoImageIndex    = resolveSource(pbr.baseColorTexture.index);
      normalImageIndex    = resolveSource(mat.normalTexture.index);
      roughnessImageIndex = resolveSource(pbr.metallicRoughnessTexture.index);
    }

    for (auto &img : gltfModel.images) {
      RawImage raw;
      raw.width  = img.width;
      raw.height = img.height;

      if (img.component == 4) {
        raw.pixels.assign(img.image.begin(), img.image.end());
      } else {
        raw.pixels.resize(img.width * img.height * 4);
        for (int i = 0; i < img.width * img.height; i++) {
          raw.pixels[i * 4 + 0] = img.component > 0 ? img.image[i * img.component + 0] : 0;
          raw.pixels[i * 4 + 1] = img.component > 1 ? img.image[i * img.component + 1] : 0;
          raw.pixels[i * 4 + 2] = img.component > 2 ? img.image[i * img.component + 2] : 0;
          raw.pixels[i * 4 + 3] = 255;
        }
      }
      images.push_back(std::move(raw));
    }
  }

} // namespace mc
