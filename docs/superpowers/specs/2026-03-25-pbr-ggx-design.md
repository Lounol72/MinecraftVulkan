# PBR GGX — Design Spec

**Date** : 2026-03-25
**Branche** : 3D
**Scope** : Direct lights uniquement (IBL = étape suivante)

---

## Contexte

Le fragment shader actuel utilise Blinn-Phong (diffuse Lambert + spéculaire `pow(NdotH, shininess)`). La texture au binding 2 est une ORM glTF (R=Occlusion, G=Roughness, B=Metallic) mais seul le channel G est lu. Le channel B (metallic) et R (AO) sont ignorés. Aucun tone mapping ni correction gamma n'est appliqué.

---

## Décisions

- **BRDF** : Cook-Torrance (NDF GGX + Smith Schlick-GGX + Fresnel Schlick)
- **Tone mapping** : ACES filmic (Hill approximation), extensible via specialization constants
- **Gamma** : correction sRGB en sortie (`pow(color, 1/2.2)`)
- **Paramètres matériau** : UBO `MaterialParams` au set 1 binding 3, conforme glTF 2.0
- **Metallic** : lu depuis channel B de l'ORM texture (était ignoré)
- **AO** : lu depuis channel R de l'ORM texture (était ignoré)

---

## Section 1 — Descriptor Set 1

| Binding | Type | Nom | Changement |
|---|---|---|---|
| 0 | `COMBINED_IMAGE_SAMPLER` | `albedoSampler` | inchangé |
| 1 | `COMBINED_IMAGE_SAMPLER` | `normalSampler` | inchangé |
| 2 | `COMBINED_IMAGE_SAMPLER` | `ormSampler` | renommé (était `roughnessSampler`) |
| 3 | `UNIFORM_BUFFER` | `MaterialParams` | **nouveau** |

### UBO MaterialParams (48 bytes, std140)

```glsl
layout(set = 1, binding = 3) uniform MaterialParams {
    vec4  baseColorFactor;  // multiplie albedo (défaut: 1,1,1,1)
    vec4  pbrFactors;       // x=metallicFactor, y=roughnessFactor, z=normalScale, w=occlusionStrength
    vec3  emissiveFactor;   // couleur émissive (défaut: 0,0,0)
    float _pad;
} mat;
```

### C++ — struct mirror

```cpp
struct MaterialParamsUBO {
    glm::vec4 baseColorFactor{1.f};
    glm::vec4 pbrFactors{1.f, 1.f, 1.f, 1.f}; // metallic, roughness, normalScale, occlusionStrength
    glm::vec3 emissiveFactor{0.f};
    float     _pad{0.f};
};
```

`MaterialInstance` possède un `mc::Buffer` (UNIFORM_BUFFER, HOST_VISIBLE) mappé en permanence. Un setter `setParams(MaterialParamsUBO)` appelle `writeToBuffer` + `flush`.

---

## Section 2 — BRDF Cook-Torrance

### Formule

```
Lo(V) = Σ [ (fd + fr) * radiance * NdotL ]  +  ambient * ao  +  emissive
```

Avec :
```
fr = D(H,α) * G(N,L,V,α) * F(V,H,F0)
     ─────────────────────────────────
              4 * NdotL * NdotV

fd = (1 - F) * (1 - metallic) * albedo / π
```

### Composantes

**D — NDF GGX (Trowbridge-Reitz)**
```glsl
float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
```

**G — Smith Schlick-GGX (version direct lights)**
```glsl
float geometrySchlickGGX(float NdotX, float roughness) {
    float k = (roughness + 1.0);
    k = (k * k) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}
float geometrySmith(float NdotL, float NdotV, float roughness) {
    return geometrySchlickGGX(NdotL, roughness) * geometrySchlickGGX(NdotV, roughness);
}
```

**F — Fresnel Schlick**
```glsl
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

**F0 (réflectance à incidence normale)**
```glsl
vec3 F0 = mix(vec3(0.04), albedo, metallic);
```

**AO**
```glsl
float ao = 1.0 + mat.pbrFactors.w * (ormSample.r - 1.0);
// ambientLight *= ao
```

---

## Section 3 — Tone Mapping & Gamma

### Specialization constant

```glsl
layout(constant_id = 0) const uint TONE_MAP_MODE = 0;
// 0 = ACES filmic
// 1 = Reinhard        (futur)
// 2 = Khronos Neutral (futur)
```

`Material` expose un enum `ToneMapMode` et passe `VkSpecializationInfo` à la création du pipeline color. Le depth pipeline n'est pas affecté.

### ACES filmic (Hill approximation)

```glsl
vec3 toneMapACES(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
```

### Gamma correction sRGB

```glsl
outColor = vec4(pow(tonemapped, vec3(1.0 / 2.2)), 1.0);
```

---

## Changements C++

| Fichier | Changement |
|---|---|
| `include/material.hpp` | Ajout enum `ToneMapMode`, `MaterialParamsUBO` struct, setter dans `MaterialInstance` |
| `src/material.cpp` | Binding 3 dans `createSet1Layout()`, `VkSpecializationInfo` dans `createPipeline()`, `Buffer` UBO dans `MaterialInstance` |
| `shaders/shader.frag` | Remplace Blinn-Phong par Cook-Torrance, ajoute tone mapping + gamma |

Aucun changement dans `frame_info.hpp`, `depth.vert/.frag`.

`app.cpp` requiert des modifications mineures : ajout du pool ratio `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` dans `materialAllocator`, passage de `Device&` au constructeur de `MaterialInstance`, renommage `roughness` → `orm`.

---

## Compatibilité ascendante

Les valeurs par défaut du `MaterialParamsUBO` (`metallicFactor=1`, `roughnessFactor=1`, `normalScale=1`, `occlusionStrength=1`, `baseColorFactor=(1,1,1,1)`) reproduisent le comportement actuel avec les textures existantes. Aucune modification de `App::run()` requise.
