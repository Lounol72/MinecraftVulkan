#pragma once

#include "camera.hpp"
#include "game_object.hpp"

// libs

#include <sys/types.h>
#include <vulkan/vulkan.h>

#define MAX_LIGHTS 10

namespace mc {

  struct SimplePushConstantsData {
    glm::mat4 modelMatrix{1.f};
    glm::mat4 normalMatrix{1.f};
  };

  struct Frustum {
    glm::vec4 planes[6];
  };

  struct PointLightData {
    glm::vec4 position; // w unused
    glm::vec4 color;    // rdb + w intensité
  };

  struct GlobalSceneData {
    glm::mat4      projection;
    glm::mat4      view;
    glm::vec4      ambientLightColor;
    PointLightData pointLights[MAX_LIGHTS];
    int            numLights;
    int            _pad[3];
  };

  struct FrameInfo {
    int              frameIndex;
    float            frameTime;
    VkCommandBuffer  commandBuffer;
    Camera          &camera;
    VkDescriptorSet  globalDescriptorSet;
    GameObject::Map &gameObjects;
  };

  inline Frustum extractFrustum(const glm::mat4 &vp) {
    glm::mat4 t = glm::transpose(vp);
    Frustum   f;
    for (int i = 0; i < 3; i++) {
      f.planes[i * 2]     = t[3] + t[i];
      f.planes[i * 2 + 1] = t[3] - t[i];
    }
    return f;
  }
} // namespace mc
