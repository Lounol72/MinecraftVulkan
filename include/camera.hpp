#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace mc {
class Camera {
public:
  void setOrthographicProjection(float left, float right, float top,
                                 float bottom, float near, float far);

  void setPerspectiveProjection(float fovy, float aspect, float near,
                                float far);

  const glm::mat4 &getProjection() const { return projectionMatrix; }

private:
  glm::mat4 projectionMatrix{1.f};
};
} // namespace mc
