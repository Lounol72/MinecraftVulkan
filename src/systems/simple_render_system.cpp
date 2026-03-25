#include "simple_render_system.hpp"
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include "frame_info.hpp"
#include "material.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace mc {

  SimpleRenderSystem::SimpleRenderSystem(Device &device)
      : device{device} {
  }
  SimpleRenderSystem::~SimpleRenderSystem() {
  }

  std::vector<GameObject *> SimpleRenderSystem::buildRenderList(FrameInfo &frameInfo) {
    std::vector<GameObject *> renderables;
    Frustum frustum = extractFrustum(frameInfo.camera.getProjection() * frameInfo.camera.getView());

    for (auto &[id, obj] : frameInfo.gameObjects) {
      if (!obj.model || !obj.materialInstance)
        continue;

      if (obj.transform.isDirty()) {
        const Model::AABB &local = obj.model->getAABB();
        const glm::mat4   &m     = obj.transform.mat4();
        obj.cachedWorldAABB      = {glm::vec3{FLT_MAX}, glm::vec3{-FLT_MAX}};
        for (int i = 0; i < 8; i++) {
          glm::vec3 corner{
              (i & 1) ? local.max.x : local.min.x,
              (i & 2) ? local.max.y : local.min.y,
              (i & 4) ? local.max.z : local.min.z,
          };
          glm::vec3 wp            = glm::vec3(m * glm::vec4(corner, 1.f));
          obj.cachedWorldAABB.min = glm::min(obj.cachedWorldAABB.min, wp);
          obj.cachedWorldAABB.max = glm::max(obj.cachedWorldAABB.max, wp);
        }
      }

      bool inside = true;
      for (const auto &plane : frustum.planes) {
        glm::vec3 positive{
            plane.x >= 0 ? obj.cachedWorldAABB.max.x : obj.cachedWorldAABB.min.x,
            plane.y >= 0 ? obj.cachedWorldAABB.max.y : obj.cachedWorldAABB.min.y,
            plane.z >= 0 ? obj.cachedWorldAABB.max.z : obj.cachedWorldAABB.min.z,
        };
        if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.f) {
          inside = false;
          break;
        }
      }
      if (inside)
        renderables.push_back(&obj);
    }

    std::sort(renderables.begin(), renderables.end(), [](const GameObject *a, const GameObject *b) {
      const Material *ma = a->materialInstance->getMaterial();
      const Material *mb = b->materialInstance->getMaterial();
      if (ma != mb)
        return ma < mb;
      return a->materialInstance.get() < b->materialInstance.get();
    });

    return renderables;
  }

  void SimpleRenderSystem::renderDepthPrePass(FrameInfo                        &frameInfo,
                                              const std::vector<GameObject *>  &renderables) {
    const Material *lastMaterial = nullptr;

    for (auto *obj : renderables) {
      const Material *mat = obj->materialInstance->getMaterial();

      if (mat != lastMaterial) {
        mat->bindDepthPipeline(frameInfo.commandBuffer);
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                obj->materialInstance->getPipelineLayout(),
                                0, 1,
                                &frameInfo.globalDescriptorSet,
                                0, nullptr);
        lastMaterial = mat;
      }

      SimplePushConstantsData push{};
      push.modelMatrix  = obj->transform.mat4();
      push.normalMatrix = glm::mat4(obj->transform.normalMatrix());
      vkCmdPushConstants(frameInfo.commandBuffer,
                         obj->materialInstance->getPipelineLayout(),
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(SimplePushConstantsData), &push);
      obj->model->bind(frameInfo.commandBuffer);
      obj->model->draw(frameInfo.commandBuffer);
    }
  }

  void SimpleRenderSystem::renderColorPass(FrameInfo                       &frameInfo,
                                           const std::vector<GameObject *> &renderables) {
    const Material         *lastMaterial         = nullptr;
    const MaterialInstance *lastMaterialInstance = nullptr;

    for (auto *obj : renderables) {
      const Material *mat = obj->materialInstance->getMaterial();

      if (mat != lastMaterial) {
        obj->materialInstance->bindPipeline(frameInfo.commandBuffer, frameInfo.globalDescriptorSet);
        // Bind IBL textures (set 2) — shared across all materials/objects.
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                obj->materialInstance->getPipelineLayout(),
                                2, 1,
                                &frameInfo.iblDescriptorSet,
                                0, nullptr);
        lastMaterial         = mat;
        lastMaterialInstance = obj->materialInstance.get();
      } else if (obj->materialInstance.get() != lastMaterialInstance) {
        obj->materialInstance->bindDescriptorSet(frameInfo.commandBuffer);
        lastMaterialInstance = obj->materialInstance.get();
      }

      SimplePushConstantsData push{};
      push.modelMatrix  = obj->transform.mat4();
      push.normalMatrix = glm::mat4(obj->transform.normalMatrix());
      vkCmdPushConstants(frameInfo.commandBuffer,
                         obj->materialInstance->getPipelineLayout(),
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                         0, sizeof(SimplePushConstantsData), &push);
      obj->model->bind(frameInfo.commandBuffer);
      obj->model->draw(frameInfo.commandBuffer);
    }
  }

  void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo) {
    auto renderables = buildRenderList(frameInfo);
    renderColorPass(frameInfo, renderables);
  }
} // namespace mc
