#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 uv;
layout (location = 4) in vec4 tangent;

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

layout(push_constant) uniform Push {
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

void main() {
  gl_Position = scene.projection * scene.view * push.modelMatrix * vec4(position, 1.0);
}
