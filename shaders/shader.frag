#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec2 fragUV;
layout (location = 3) in mat3 fragTBN;

layout (location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler2D albedoSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;
layout (set = 1, binding = 2) uniform sampler2D roughnessSampler;

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

// TBN calculé par dérivées écran — fonctionne sans attribut TANGENT
mat3 computeTBN(vec3 N) {
  vec3  q1  = dFdx(fragPosWorld);
  vec3  q2  = dFdy(fragPosWorld);
  vec2  st1 = dFdx(fragUV);
  vec2  st2 = dFdy(fragUV);
  float det = st1.s * st2.t - st2.s * st1.t;
  vec3  T   = normalize((q1 * st2.t - q2 * st1.t) / det);
  T         = normalize(T - dot(T, N) * N);
  vec3  B   = cross(N, T);
  return mat3(T, B, N);
}

void main() {
  vec3 N_geom = normalize(fragTBN[2]);
  mat3 TBN    = computeTBN(N_geom);

  vec3 normalSample = texture(normalSampler, fragUV).rgb * 2.0 - 1.0;
  vec3 N            = normalize(TBN * normalSample);

  // roughness depuis channel G (metallic-roughness glTF)
  float roughness = texture(roughnessSampler, fragUV).g;
  float shininess = pow(1.0 - roughness, 2.0) * 256.0 + 1.0;

  // Position caméra extraite de la view matrix (sans inverse() coûteux)
  vec3 cameraPos = -(transpose(mat3(scene.view)) * scene.view[3].xyz);
  vec3 viewDir   = normalize(cameraPos - fragPosWorld);

  vec3 ambientLight  = scene.ambientLightColor.xyz * scene.ambientLightColor.w;
  vec3 diffuseLight  = ambientLight;
  vec3 specularLight = vec3(0.0);

  for (int i = 0; i < scene.numLights; i++) {
    vec3  dirToLight  = scene.pointLights[i].position.xyz - fragPosWorld;
    float attenuation = 1.0 / dot(dirToLight, dirToLight);
    vec3  L           = normalize(dirToLight);
    float cosAngle    = max(dot(N, L), 0.0);

    vec3 lightContrib = scene.pointLights[i].color.xyz * scene.pointLights[i].color.w * attenuation;
    diffuseLight  += lightContrib * cosAngle;

    vec3  H         = normalize(L + viewDir);
    float specAngle = max(dot(N, H), 0.0);
    specularLight  += lightContrib * pow(specAngle, shininess);
  }

  vec3 albedo = texture(albedoSampler, fragUV).rgb;
  outColor = vec4(diffuseLight * albedo + specularLight, 1.0);
}
