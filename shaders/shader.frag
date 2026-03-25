#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec2 fragUV;
layout (location = 3) in mat3 fragTBN;

layout (location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform sampler2D albedoSampler;
layout (set = 1, binding = 1) uniform sampler2D normalSampler;
layout (set = 1, binding = 2) uniform sampler2D ormSampler;
layout (set = 1, binding = 4) uniform sampler2D emissiveSampler;

layout (set = 1, binding = 3) uniform MaterialParams {
    vec4 baseColorFactor;
    vec4 pbrFactors; // x=metallic, y=roughness, z=normalScale, w=occlusionStrength
    vec3 emissiveFactor;
    float _pad;
} mat;

layout (set = 2, binding = 0) uniform samplerCube irradianceMap;
layout (set = 2, binding = 1) uniform samplerCube prefilteredMap;
layout (set = 2, binding = 2) uniform sampler2D brdfLut;

struct PointLightData {
    vec4 position;
    vec4 color;
};

layout(std430, set = 0, binding = 0) readonly buffer GlobalSceneData {
    mat4 projection;
    mat4 view;
    vec4 ambientLightColor;
    PointLightData pointLights[10];
    int numLights;
} scene;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

layout(constant_id = 0) const uint TONE_MAP_MODE = 0;
// 0 = ACES filmic, 1 = Reinhard

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// ─── TBN ────────────────────────────────────────────────────────────────────
mat3 buildTBN() {
    vec3 N = normalize(fragTBN[2]);
    vec3 T = normalize(fragTBN[0]);
    T = normalize(T - dot(T, N) * N);
    float handedness = sign(dot(fragTBN[1], cross(fragTBN[2], fragTBN[0])));
    vec3 B = cross(N, T) * handedness;
    return mat3(T, B, N);
} // ← FIX : accolade fermante

// ─── BRDF Cook-Torrance ──────────────────────────────────────────────────────
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-7);
} // ← FIX

float geometrySchlickGGX(float NdotX, float roughness) {
    float k = (roughness + 1.0);
    k = (k * k) / 8.0;
    return NdotX / max(NdotX * (1.0 - k) + k, 1e-7);
} // ← FIX

float geometrySmith(float NdotL, float NdotV, float roughness) {
    return geometrySchlickGGX(NdotL, roughness) * geometrySchlickGGX(NdotV, roughness);
} // ← FIX

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} // ← FIX

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} // ← FIX

// ─── Tone mapping ────────────────────────────────────────────────────────────
vec3 toneMapACES(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
} // ← FIX

vec3 toneMapReinhard(vec3 x) {
    return x / (x + vec3(1.0));
} // ← FIX

vec3 applyToneMap(vec3 color) {
    if (TONE_MAP_MODE == 1) return toneMapReinhard(color);
    else return toneMapACES(color);
} // ← FIX

// ─── Main ────────────────────────────────────────────────────────────────────
void main() {
    mat3 TBN = buildTBN();
    vec3 normalSample = texture(normalSampler, fragUV).rgb * 2.0 - 1.0;
    normalSample.xy *= mat.pbrFactors.z; // normalScale

    // FIX : variance calculée sur N_raw (avant normalisation) pour éviter la pixelisation
    vec3 N_raw = TBN * normalSample;
    float variance = dot(dFdx(N_raw), dFdx(N_raw)) + dot(dFdy(N_raw), dFdy(N_raw));
    vec3 N = normalize(N_raw);

    // ORM : R=AO, G=roughness, B=metallic
    vec3 ormSample = texture(ormSampler, fragUV).rgb;
    float ao = 1.0 + mat.pbrFactors.w * (ormSample.r - 1.0);
    float roughness = clamp(ormSample.g * mat.pbrFactors.y, 0.04, 1.0);
    float metallic  = clamp(ormSample.b * mat.pbrFactors.x, 0.0, 1.0);

    // Specular anti-aliasing (Kaplanyan 2016)
    float r2 = roughness * roughness;
    roughness = sqrt(clamp(r2 + variance, r2, 1.0));

    // Albedo
    vec3 albedo = texture(albedoSampler, fragUV).rgb * mat.baseColorFactor.rgb;

    // F0 : diélectrique = 0.04, métal = albedo
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Vue
    vec3 cameraPos = -(transpose(mat3(scene.view)) * scene.view[3].xyz);
    vec3 V = normalize(cameraPos - fragPosWorld);
    float NdotV = max(dot(N, V), 1e-4);

    // Accumulation des lumières directes
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < scene.numLights; i++) {
        vec3 dirToLight = scene.pointLights[i].position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(dirToLight, dirToLight);
        vec4 lightColor = scene.pointLights[i].color;
        vec3 L = normalize(dirToLight);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        vec3 radiance = lightColor.rgb * lightColor.a * attenuation;

        float D   = distributionGGX(NdotH, roughness);
        float G   = geometrySmith(NdotL, NdotV, roughness);
        vec3  F   = fresnelSchlick(HdotV, F0);
        vec3  fr  = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-7);

        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 fd = kD * albedo / PI;

        Lo += (fd + fr) * radiance * NdotL;
    } // ← FIX : accolade fermante du for

    // ─── IBL ambient ──────────────────────────────────────────────────────────
    vec3 F_ibl  = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - metallic);

    vec3 irradiance   = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL   = kD_ibl * irradiance * albedo;

    vec3 R               = reflect(-V, N);
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdfSample      = texture(brdfLut, vec2(NdotV, roughness)).rg;
    vec3 specularIBL     = prefilteredColor * (F_ibl * brdfSample.x + brdfSample.y);

    vec3 ambient  = (diffuseIBL + specularIBL) * ao;
    vec3 emissive = texture(emissiveSampler, fragUV).rgb * mat.emissiveFactor;
    vec3 color    = ambient + Lo + emissive;

    // Tone mapping + gamma
    color = applyToneMap(color);
    outColor = vec4(pow(color, vec3(1.0 / 2.2)), 1.0);
}
