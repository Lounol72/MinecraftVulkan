#version 450

const vec2 OFFSETS[6] = vec2[](
  vec2(-1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, -1.0),
  vec2(1.0, -1.0),
  vec2(-1.0, 1.0),
  vec2(1.0, 1.0)
);

layout (location = 0) out vec2 fragOffset;
layout (location = 1) out flat int lightIndex;

struct PointLightData {
  vec4 position;
  vec4 color;
};

layout(std430, set = 0, binding = 0) readonly buffer GlobalSceneData {
  mat4           projection;
  mat4           view;
  vec4           ambientLightColor;
  PointLightData pointLights[10];
  int            numLights;
} scene;

const float LIGHT_RADIUS = 0.1;

void main() {
  fragOffset = OFFSETS[gl_VertexIndex];
  lightIndex = gl_InstanceIndex;

  vec3 cameraRightWorld = {scene.view[0][0], scene.view[1][0], scene.view[2][0]};
  vec3 cameraUpWorld    = {scene.view[0][1], scene.view[1][1], scene.view[2][1]};

  vec3 positionWorld = scene.pointLights[gl_InstanceIndex].position.xyz
    + LIGHT_RADIUS * fragOffset.x * cameraRightWorld
    + LIGHT_RADIUS * fragOffset.y * cameraUpWorld;

  gl_Position = scene.projection * scene.view * vec4(positionWorld, 1.0);
}
