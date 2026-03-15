# Uniform Buffer Object (UBO)

Guide d'implémentation d'un UBO dans ce projet pour passer des données globales
(résolution, temps, etc.) aux shaders.

---

## Vue d'ensemble

Un UBO, c'est un buffer côté GPU que le shader peut lire en lecture seule via un
`binding`. Le CPU écrit dedans chaque frame via un simple `memcpy`.

Le flux complet :

```
CPU (struct GlobalUBO)
  → vkMapMemory / memcpy
    → VkBuffer (GPU)
      → VkDescriptorSet (binding=0)
        → shader (layout binding=0)
```

---

## Étape 1 — Définir la struct UBO

Dans `include/app.hpp` (ou un header dédié) :

```cpp
#include <glm/glm.hpp>

struct GlobalUBO {
    glm::vec2 iResolution;
    float     iTime;
    float     _pad; // aligne à 16 bytes (règle std140)
};
```

**Règle std140** : Vulkan exige que les membres d'un UBO respectent un alignement
strict. Un `vec2` fait 8 bytes, suivi d'un `float` à 4 bytes — on ajoute 4 bytes
de padding pour atteindre 16 bytes et respecter l'alignement du bloc.

---

## Étape 2 — Créer le VkDescriptorSetLayout

Le layout décrit ce que le shader va recevoir : "au binding 0, il y a un UBO,
visible depuis le fragment shader".

À faire une seule fois, dans `App::createPipelineLayout()` ou une méthode dédiée.

```cpp
// Dans app.hpp — ajouter le membre :
VkDescriptorSetLayout descriptorSetLayout;

// Dans app.cpp :
void App::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;

    vkCreateDescriptorSetLayout(device.device(), &layoutInfo, nullptr,
                                &descriptorSetLayout);
}
```

Puis passer ce layout au `VkPipelineLayout` dans `createPipelineLayout()` :

```cpp
pipelineLayoutInfo.setLayoutCount = 1;
pipelineLayoutInfo.pSetLayouts    = &descriptorSetLayout;
```

Détruire dans le destructeur :

```cpp
vkDestroyDescriptorSetLayout(device.device(), descriptorSetLayout, nullptr);
```

---

## Étape 3 — Créer les VkBuffer (un par image swapchain)

On crée **un buffer par image swapchain** pour éviter d'écrire dans un buffer
pendant qu'il est utilisé par le GPU.

```cpp
// Dans app.hpp :
std::vector<VkBuffer>       uboBuffers;
std::vector<VkDeviceMemory> uboMemory;

// Dans app.cpp :
void App::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(GlobalUBO);
    int imageCount = swapChain.imageCount();

    uboBuffers.resize(imageCount);
    uboMemory.resize(imageCount);

    for (int i = 0; i < imageCount; i++) {
        // device.createBuffer est une helper classique dans les tutos Vulkan
        // (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | HOST_VISIBLE | HOST_COHERENT)
        device.createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uboBuffers[i],
            uboMemory[i]
        );
    }
}
```

`HOST_VISIBLE` = le CPU peut écrire dedans.
`HOST_COHERENT` = pas besoin de flush manuel après l'écriture.

Détruire dans le destructeur :

```cpp
for (int i = 0; i < uboBuffers.size(); i++) {
    vkDestroyBuffer(device.device(), uboBuffers[i], nullptr);
    vkFreeMemory(device.device(), uboMemory[i], nullptr);
}
```

---

## Étape 4 — Créer le VkDescriptorPool

Le pool alloue les descriptor sets. Il doit savoir combien de descripteurs de
chaque type il va héberger.

```cpp
// Dans app.hpp :
VkDescriptorPool descriptorPool;

// Dans app.cpp :
void App::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(swapChain.imageCount());

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = static_cast<uint32_t>(swapChain.imageCount());

    vkCreateDescriptorPool(device.device(), &poolInfo, nullptr, &descriptorPool);
}
```

Détruire dans le destructeur :

```cpp
vkDestroyDescriptorPool(device.device(), descriptorPool, nullptr);
```

---

## Étape 5 — Allouer et écrire les VkDescriptorSet

Un descriptor set par image, chacun pointant vers le buffer correspondant.

```cpp
// Dans app.hpp :
std::vector<VkDescriptorSet> descriptorSets;

// Dans app.cpp :
void App::createDescriptorSets() {
    int imageCount = swapChain.imageCount();
    std::vector<VkDescriptorSetLayout> layouts(imageCount, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(imageCount);
    allocInfo.pSetLayouts        = layouts.data();

    descriptorSets.resize(imageCount);
    vkAllocateDescriptorSets(device.device(), &allocInfo, descriptorSets.data());

    // Pointer chaque descriptor set vers son buffer
    for (int i = 0; i < imageCount; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uboBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(GlobalUBO);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSets[i];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
    }
}
```

---

## Étape 6 — Mettre à jour les données chaque frame

Dans `App::drawFrame()`, avant ou après `acquireNextImage` :

```cpp
void App::updateUniformBuffer(uint32_t imageIndex) {
    GlobalUBO ubo{};
    ubo.iResolution = {swapChain.width(), swapChain.height()};
    ubo.iTime       = static_cast<float>(glfwGetTime());

    void* data;
    vkMapMemory(device.device(), uboMemory[imageIndex], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device.device(), uboMemory[imageIndex]);
}
```

`glfwGetTime()` retourne le temps écoulé en secondes depuis l'initialisation de
GLFW — pratique pour animer les shaders.

---

## Étape 7 — Binder dans le command buffer

Dans `createCommandBuffers()`, juste avant `model->draw()` :

```cpp
vkCmdBindDescriptorSets(
    commandBuffers[i],
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0,        // firstSet
    1,        // descriptorSetCount
    &descriptorSets[i],
    0, nullptr
);
```

---

## Étape 8 — Le shader

```glsl
// shader.frag
#version 450

layout(binding = 0) uniform GlobalUBO {
    vec2  iResolution;
    float iTime;
    float _pad;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = gl_FragCoord.xy / ubo.iResolution;
    outColor = vec4(uv, abs(sin(ubo.iTime)), 1.0);
}
```

---

## Ordre d'initialisation dans App::App()

```cpp
App::App() {
    createDescriptorSetLayout(); // avant createPipelineLayout
    loadModels();
    createPipelineLayout();      // utilise descriptorSetLayout
    createPipeline();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();      // utilise descriptorSets
}
```

---

## Ajouter une nouvelle variable globale plus tard

Il suffit d'ajouter un champ dans `GlobalUBO` (en respectant l'alignement std140)
et de le remplir dans `updateUniformBuffer()`. Le layout, le pool, les descriptor
sets — rien d'autre ne change.

```cpp
struct GlobalUBO {
    glm::vec2 iResolution;
    float     iTime;
    float     _pad;
    glm::vec4 iMouse; // ← ajout : x, y, clic gauche, clic droit
};
```
