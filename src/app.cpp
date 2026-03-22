#include "app.hpp"

#include <vulkan/vulkan_core.h>
#include "buffer.hpp"
#include "camera.hpp"
#include "descriptors.hpp"
#include "frame_info.hpp"
#include "keyboard_movement_controller.hpp"
#include "point_light_system.hpp"
#include "simple_render_system.hpp"
#include "swap_chain.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <chrono>
#include <memory>

namespace mc {

  struct GlobalUBO {
    glm::mat4 projection{1.f};
    glm::mat4 view{1.f};
    glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};
    glm::vec3 lightPosition{-1.f};
    alignas(16) glm::vec4 lightColor{1.f}; // w is light intensity
  };

  App::App() {
    globalPool =
        DescriptorPool::Builder(device)
            .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
            .build();
    loadGameObjects();
  }
  App::~App() {
  }
  void App::run() {
    std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<Buffer>(device,
                                               sizeof(GlobalUBO),
                                               1,
                                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      uboBuffers[i]->map();
    }

    auto globalSetLayout =
        DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      DescriptorWriter(*globalSetLayout, *globalPool)
          .writeBuffer(0, &bufferInfo)
          .build(globalDescriptorSets[i]);
    }

    SimpleRenderSystem simpleRenderSystem{device,
                                          renderer.getSwapChainRenderPass(),
                                          globalSetLayout->getDescriptorSetLayout()};
    PointLightSystem   pointLightSystem{device,
                                        renderer.getSwapChainRenderPass(),
                                        globalSetLayout->getDescriptorSetLayout()};
    Camera             camera{};

    camera.setViewTarget(glm::vec3(-1.f, -2.f, 2.f), glm::vec3(0.f, 0.f, 2.5f));

    auto viewerObject                    = GameObject::createGameObject();
    viewerObject.transform.translation.z = -2.5f;
    KeyBoardMovementController cameraController{};

    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!window.shouldClose()) {
      glfwPollEvents();

      auto  newTime = std::chrono::high_resolution_clock::now();
      float frameTime =
          std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;

      cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime, viewerObject);
      camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

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
        GlobalUBO ubo{};
        ubo.projection = camera.getProjection();
        ubo.view       = camera.getView();
        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

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
    std::shared_ptr<Model> model = Model::createModelFromFile(device, "models/smooth_vase.obj");

    auto smoothVase                  = GameObject::createGameObject();
    smoothVase.model                 = model;
    smoothVase.transform.translation = {-.5f, .5f, 0.f};
    smoothVase.transform.scale       = {3.f, 1.5f, 3.f};
    gameObjects.emplace(smoothVase.getId(), std::move(smoothVase));

    model                          = Model::createModelFromFile(device, "models/flat_vase.obj");
    auto flatVase                  = GameObject::createGameObject();
    flatVase.model                 = model;
    flatVase.transform.translation = {.5f, .5f, 0.f};
    flatVase.transform.scale       = {3.f, 1.5f, 3.f};
    gameObjects.emplace(flatVase.getId(), std::move(flatVase));

    model                       = Model::createModelFromFile(device, "models/quad.obj");
    auto floor                  = GameObject::createGameObject();
    floor.model                 = model;
    floor.transform.translation = {0.f, .5f, 0.f};
    floor.transform.scale       = {3.f, 1.f, 3.f};
    gameObjects.emplace(floor.getId(), std::move(floor));
  }
} // namespace mc
