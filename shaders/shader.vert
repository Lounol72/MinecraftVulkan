#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 uv;
layout (location = 4) in vec4 tangent;

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec3 fragPosWorld;
layout (location = 2) out vec2 fragUV;
layout (location = 3) out mat3 fragTBN;

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

layout(push_constant) uniform Push{
  mat4 modelMatrix;
  mat4 normalMatrix;
} push;

void main() {
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  gl_Position = scene.projection * scene.view * positionWorld;

  mat3 normalMat = mat3(push.normalMatrix);
  vec3 N = normalize(normalMat * normal);
  vec3 T = normalize(normalMat * tangent.xyz);
  T = normalize(T - dot(T, N) * N); // re-orthogonalisation de Gram-Schmidt
  vec3 B = cross(N, T) * tangent.w; // tangent.w = handedness (-1 ou +1)

  fragTBN      = mat3(T, B, N);
  fragPosWorld = positionWorld.xyz;
  fragColor    = color;
  fragUV       = uv;
}
