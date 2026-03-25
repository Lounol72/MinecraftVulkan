#version 450

layout (location = 0) in vec2 fragOffset;
layout (location = 1) in flat int lightIndex;

layout (location = 0) out vec4 outColor;

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

void main() {
  float dist = sqrt(dot(fragOffset, fragOffset));
  if (dist >= 1.0) {
    discard;
  }
  outColor = vec4(scene.pointLights[lightIndex].color.xyz, 1.0);
}
