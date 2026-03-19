#include "app.hpp"

#include "camera.hpp"
#include "keyboard_movement_controller.hpp"
#include "simple_render_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <chrono>
#include <memory>

namespace mc {

App::App() { loadGameObjects(); }
App::~App() {}
void App::run() {

  SimpleRenderSystem simpleRenderSystem{device,
                                        renderer.getSwapChainRenderPass()};
  Camera camera{};

  camera.setViewTarget(glm::vec3(-1.f, -2.f, 2.f), glm::vec3(0.f, 0.f, 2.5f));

  auto viewerObject = GameObject::createGameObject();
  KeyBoardMovementController cameraController{};

  auto currentTime = std::chrono::high_resolution_clock::now();

  while (!window.shouldClose()) {
    glfwPollEvents();

    auto newTime = std::chrono::high_resolution_clock::now();
    float frameTime =
        std::chrono::duration<float, std::chrono::seconds::period>(newTime -
                                                                   currentTime)
            .count();
    currentTime = newTime;

    cameraController.moveInPlaneXZ(window.getGLFWwindow(), frameTime,
                                   viewerObject);
    camera.setViewYXZ(viewerObject.transform.translation,
                      viewerObject.transform.rotation);

    float aspectRatio = renderer.getAspectRatio();

    camera.setPerspectiveProjection(glm::radians(50.f), aspectRatio, 0.1f,
                                    10.f);

    if (auto commandBuffer = renderer.beginFrame()) {
      renderer.beginSwapChainRenderPass(commandBuffer);
      simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
      renderer.endSwapChainRenderPass(commandBuffer);
      renderer.endFrame();
    }
  }
  vkDeviceWaitIdle(device.device());
}
void App::loadGameObjects() {
  std::shared_ptr<Model> Gamemodel =
      Model::createModelFromFile(device, "models/smooth_vase.obj");

  auto gameObject = GameObject::createGameObject();
  gameObject.model = Gamemodel;
  gameObject.transform.translation = {.0f, .0f, 2.5f};
  gameObject.transform.scale = glm::vec3(3.f);
  gameObjects.push_back(std::move(gameObject));
}
} // namespace mc
