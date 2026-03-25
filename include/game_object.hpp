#pragma once

#include "material.hpp"
#include "model.hpp"

// libs
#include <glm/gtc/matrix_transform.hpp>
// std
#include <memory>
#include <optional>
#include <unordered_map>

namespace mc {
  struct PointLightComponent {
    glm::vec4 color{1.f};
  };

  struct TransformComponent {
    void      setTranslation(glm::vec3 t) { translation = t; dirty = true; }
    void      setScale(glm::vec3 s)       { scale = s;       dirty = true; }
    void      setRotation(glm::vec3 r)    { rotation = r;    dirty = true; }
    glm::vec3 getTranslation() const { return translation; }
    glm::vec3 getScale()       const { return scale; }
    glm::vec3 getRotation()    const { return rotation; }

    bool      isDirty() const { return dirty; }
    glm::mat4 mat4();
    glm::mat3 normalMatrix();

  private:
    glm::vec3 translation{};
    glm::vec3 scale{1.f, 1.f, 1.f};
    glm::vec3 rotation{};

    bool      dirty        = true;
    glm::mat4 cachedMat4   {1.f};
    glm::mat3 cachedNormal {1.f};
  };

  class GameObject {
  public:
    using id_t = unsigned int;
    using Map  = std::unordered_map<id_t, GameObject>;

    static GameObject createGameObject() {
      static id_t currentId = 0;
      return GameObject{currentId++};
    }

    GameObject(const GameObject &)            = delete;
    GameObject &operator=(const GameObject &) = delete;
    GameObject(GameObject &&)                 = default;
    GameObject &operator=(GameObject &&)      = default;

    const id_t getId() {
      return id;
    }

    std::shared_ptr<Model>             model{};
    glm::vec3                          color{};
    TransformComponent                 transform{};
    std::optional<PointLightComponent> pointLight{};
    std::shared_ptr<MaterialInstance>  materialInstance{};
    Model::AABB                        cachedWorldAABB{};

  private:
    GameObject(id_t objId)
        : id{objId} {
    }

    id_t id;
  };
} // namespace mc
