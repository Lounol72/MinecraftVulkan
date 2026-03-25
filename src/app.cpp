#include "app.hpp"

#include <stb_image.h>
#include <vulkan/vulkan_core.h>
#include <glm/ext/scalar_constants.hpp>
#include "buffer.hpp"
#include "camera.hpp"
#include "descriptors.hpp"
#include "frame_info.hpp"
#include "game_object.hpp"
#include "keyboard_movement_controller.hpp"
#include "material.hpp"
#include "point_light_system.hpp"
#include "simple_render_system.hpp"
#include "swap_chain.hpp"
#include "texture.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <chrono>
#include <memory>

namespace mc {

  App::App() {
    globalPool =
        DescriptorPool::Builder(device)
            .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();

    stbi_uc whitePixel[4]      = {255, 255, 255, 255};
    whiteTexture               = std::make_shared<Texture>(device, 1, 1, whitePixel);
    stbi_uc flatNormalPixel[4] = {128, 128, 255, 255};
    flatNormalTexture          = std::make_shared<Texture>(device, 1, 1, flatNormalPixel);

    materialAllocator = std::make_unique<DescriptorAllocatorGrowable>();
    materialAllocator->init(device, 32, {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.f}});

    loadGameObjects();
  }
  App::~App() {
    gameObjects.clear();
    whiteTexture.reset();
    defaultMaterial.reset();
    flatNormalTexture.reset();
    if (materialAllocator)
      materialAllocator->destroyPools();
  }
  void App::run() {
    std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<Buffer>(device,
                                               sizeof(GlobalSceneData),
                                               1,
                                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
      uboBuffers[i]->map();
    }

    auto globalSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      DescriptorWriter(*globalSetLayout, *globalPool)
          .writeBuffer(0, &bufferInfo)
          .build(globalDescriptorSets[i]);
    }

    defaultMaterial = std::make_unique<Material>(device,
                                                 renderer.getSwapChainRenderPass(),
                                                 globalSetLayout->getDescriptorSetLayout());

    for (auto &[id, obj] : gameObjects) {
      if (!obj.model)
        continue;
      auto albedo    = obj.model->getAlbedoTexture(whiteTexture);
      auto normal    = obj.model->getNormalTexture(flatNormalTexture);
      auto roughness = obj.model->getRoughnessTexture(whiteTexture);
      obj.materialInstance = std::make_shared<MaterialInstance>(*defaultMaterial,
                                                                *materialAllocator,
                                                                albedo,
                                                                normal,
                                                                roughness);
    }

    SimpleRenderSystem simpleRenderSystem{device};
    PointLightSystem   pointLightSystem{device,
                                        renderer.getSwapChainRenderPass(),
                                        globalSetLayout->getDescriptorSetLayout()};
    Camera             camera{};

    camera.setViewTarget(glm::vec3(-1.f, -2.f, 2.f), glm::vec3(0.f, 0.f, 2.5f));

    auto viewerObject = GameObject::createGameObject();
    viewerObject.transform.setTranslation({0.f, 0.f, -2.5f});
    KeyBoardMovementController cameraController{};

    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
      glfwPollEvents();

      auto  newTime = std::chrono::high_resolution_clock::now();
      float frameTime =
          std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;

      cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewerObject);
      camera.setViewYXZ(viewerObject.transform.getTranslation(),
                        viewerObject.transform.getRotation());

      float aspectRatio = renderer.getAspectRatio();

      camera.setPerspectiveProjection(glm::radians(50.f), aspectRatio, 0.1f, 100.f);

      if (auto commandBuffer = renderer.beginFrame()) {
        int       frameIndex = renderer.getFrameIndex();
        FrameInfo frameInfo{frameIndex,
                            frameTime,
                            commandBuffer,
                            camera,
                            globalDescriptorSets[frameIndex],
                            gameObjects};
        // update
        GlobalSceneData ssbo{};
        ssbo.projection        = camera.getProjection();
        ssbo.view              = camera.getView();
        ssbo.ambientLightColor = {1.f, 1.f, 1.f, 0.05f};
        pointLightSystem.update(frameInfo, ssbo);
        uboBuffers[frameIndex]->writeToBuffer(&ssbo);

        // render
        renderer.beginSwapChainRenderPass(commandBuffer);
        simpleRenderSystem.renderGameObjects(frameInfo);
        pointLightSystem.render(frameInfo);
        renderer.endSwapChainRenderPass(commandBuffer);
        renderer.endFrame();
      }
    }
    vkDeviceWaitIdle(device.device());
  }
  void App::loadGameObjects() {
    std::shared_ptr<Model> model = Model::createModelFromFile(device, "models/DamagedHelmet.glb");

    auto smoothVase  = GameObject::createGameObject();
    smoothVase.model = model;
    smoothVase.transform.setTranslation({.0f, .0f, 0.f});
    smoothVase.transform.setScale({.5f, .5f, .5f});
    smoothVase.transform.setRotation({glm::half_pi<float>() + glm::pi<float>(), 0.f, 0.f});
    gameObjects.emplace(smoothVase.getId(), std::move(smoothVase));

    auto pointLight       = GameObject::createGameObject();
    pointLight.pointLight = PointLightComponent();
    pointLight.transform.setTranslation({0.f, -0.5f, -1.f});
    gameObjects.emplace(pointLight.getId(), std::move(pointLight));
  }
} // namespace mc
