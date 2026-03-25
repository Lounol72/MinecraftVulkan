#pragma once

#include "buffer.hpp"
#include "device.hpp"
#include "texture.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <memory>
#include <vector>

namespace mc {

  class Model {
  public:
    struct AABB {
      glm::vec3 min{FLT_MAX};
      glm::vec3 max{-FLT_MAX};
    };

    struct Vertex {
      glm::vec3 position{};
      glm::vec3 color{};
      glm::vec3 normal{};
      glm::vec2 uv{};
      glm::vec4 tangent{};

      static std::vector<VkVertexInputBindingDescription>   getBindingDescriptions();
      static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

      bool operator==(const Vertex &other) const {
        return position == other.position && color == other.color && normal == other.normal &&
               uv == other.uv && tangent == other.tangent;
      }
    };

    struct RawImage {
      int                  width, height;
      std::vector<stbi_uc> pixels; // RGBA
    };

    struct Builder {
      std::vector<Vertex>   vertices{};
      std::vector<uint32_t> indices{};
      std::vector<RawImage> images;
      int                   albedoImageIndex{-1};
      int                   normalImageIndex{-1};
      int                   roughnessImageIndex{-1};

      void loadModel(const std::string &filePath);

    private:
      void loadObj(const std::string &filePath);
      void loadGltf(const std::string &filePath);
    };

    Model(Device &inDevice, const Model::Builder &builder);
    ~Model();

    Model(const Model &)            = delete;
    Model &operator=(const Model &) = delete;

    static std::unique_ptr<Model> createModelFromFile(Device &device, const std::string &filePath);

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

    const AABB &getAABB() const { return aabb; }

    std::shared_ptr<Texture> getAlbedoTexture(std::shared_ptr<Texture> fallback) const {
      return namedTexture(albedoIndex, fallback);
    }
    std::shared_ptr<Texture> getNormalTexture(std::shared_ptr<Texture> fallback) const {
      return namedTexture(normalIndex, fallback);
    }
    std::shared_ptr<Texture> getRoughnessTexture(std::shared_ptr<Texture> fallback) const {
      return namedTexture(roughnessIndex, fallback);
    }

  private:
    void createVertexBuffers(const std::vector<Vertex> &vertices);
    void createIndexBuffers(const std::vector<uint32_t> &indices);

    std::shared_ptr<Texture> namedTexture(int index, std::shared_ptr<Texture> fallback) const {
      if (index >= 0 && index < (int)textures.size())
        return textures[index];
      return fallback;
    }

    VkIndexType indexType;

    Device &device;

    std::unique_ptr<Buffer>               vertexBuffer{};
    uint32_t                              vertexCount;
    std::vector<std::shared_ptr<Texture>> textures;
    int                                   albedoIndex{-1};
    int                                   normalIndex{-1};
    int                                   roughnessIndex{-1};

    bool                    hasIndexBuffer = false;
    std::unique_ptr<Buffer> indexBuffer{};
    uint32_t                indexCount;
    AABB                    aabb;
  };
} // namespace mc
