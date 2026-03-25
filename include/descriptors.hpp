#pragma once

#include "device.hpp"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace mc {

  // Décrit la structure d'un descriptor set : quels bindings, quels types, quels stages.
  // Immuable après création — partageable entre plusieurs pools et writers.
  // Utilise le pattern Builder pour une construction lisible :
  //   DescriptorSetLayout::Builder{device}
  //     .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
  //     .build();
  class DescriptorSetLayout {
  public:
    // Construit incrementalement un DescriptorSetLayout en chaînant les bindings.
    class Builder {
    public:
      Builder(Device &device)
          : device{device} {
      }

      // Enregistre un binding : index, type de descriptor, stages qui y accèdent,
      // et nombre de descriptors (arrays de textures, etc.).
      Builder &addBinding(uint32_t           binding,
                          VkDescriptorType   descriptorType,
                          VkShaderStageFlags stageFlags,
                          uint32_t           count = 1);

      // Crée et retourne le layout Vulkan. À appeler une seule fois.
      std::unique_ptr<DescriptorSetLayout> build() const;

    private:
      Device                                                    &device;
      std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
    };

    DescriptorSetLayout(Device                                                    &device,
                        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const DescriptorSetLayout &)            = delete;
    DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

    // Handle Vulkan brut, nécessaire pour vkAllocateDescriptorSets et la création de pipeline.
    VkDescriptorSetLayout getDescriptorSetLayout() const {
      return descriptorSetLayout;
    }
    Device &getDevice() const {
      return device;
    }

  private:
    Device               &device;
    VkDescriptorSetLayout descriptorSetLayout;

    // Conservé pour permettre au DescriptorWriter de valider les bindings à l'écriture.
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    friend class DescriptorWriter;
  };

  // Pool Vulkan à capacité fixe. Adapté aux allocations dont le nombre maximum
  // est connu à l'avance (ex : descriptor sets globaux, MAX_FRAMES_IN_FLIGHT UBOs).
  // Pour des allocations dynamiques en nombre inconnu, préférer DescriptorAllocatorGrowable.
  class DescriptorPool {
  public:
    // Construit incrémentalement un DescriptorPool en déclarant les types et quantités
    // de descriptors qu'il devra contenir.
    class Builder {
    public:
      Builder(Device &device)
          : device{device} {
      }

      // Réserve 'count' descriptors du type donné dans le pool.
      Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);

      // Flags Vulkan optionnels, ex: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
      // pour autoriser la libération individuelle de sets.
      Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);

      // Nombre maximum de descriptor sets allouables depuis ce pool.
      Builder &setMaxSets(uint32_t count);

      std::unique_ptr<DescriptorPool> build() const;

    private:
      Device                           &device;
      std::vector<VkDescriptorPoolSize> poolSizes{};
      uint32_t                          maxSets   = 1000;
      VkDescriptorPoolCreateFlags       poolFlags = 0;
    };

    DescriptorPool(Device                                  &device,
                   uint32_t                                 maxSets,
                   VkDescriptorPoolCreateFlags              poolFlags,
                   const std::vector<VkDescriptorPoolSize> &poolSizes);
    ~DescriptorPool();
    DescriptorPool(const DescriptorPool &)            = delete;
    DescriptorPool &operator=(const DescriptorPool &) = delete;

    // Tente d'allouer un descriptor set depuis ce pool.
    // Retourne false si le pool est plein — aucune exception levée.
    bool allocateDescriptor(const VkDescriptorSetLayout descriptorSetLayout,
                            VkDescriptorSet            &descriptor) const;

    // Libère individuellement des descriptor sets (nécessite
    // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).
    void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;

    // Remet le pool à zéro : tous les sets alloués deviennent invalides.
    // Plus efficace que de libérer les sets un par un.
    void resetPool();

  private:
    Device          &device;
    VkDescriptorPool descriptorPool;

    friend class DescriptorWriter;
  };

  // Allocateur de descriptor sets à capacité dynamique.
  // Gère en interne plusieurs VkDescriptorPool : quand un pool est plein,
  // un nouveau est créé automatiquement. La taille des pools créés double
  // à chaque fois (plafonnée à 4096 sets) pour limiter le nombre d'allocations.
  //
  // Utilisation typique pour les matériaux :
  //   DescriptorAllocatorGrowable allocator;
  //   allocator.init(device, 32, {
  //     { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1.f },
  //     { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2.f },
  //   });
  //   allocator.allocate(layout, set);
  //   // ... en fin de vie :
  //   allocator.destroyPools();
  class DescriptorAllocatorGrowable {
  public:
    // Proportion de descriptors d'un type donné par rapport au nombre de sets.
    // Ex : ratio = 2.f pour COMBINED_IMAGE_SAMPLER signifie 2 samplers par set en moyenne.
    struct PoolSizeRatio {
      VkDescriptorType type;
      float            ratio;
    };

    DescriptorAllocatorGrowable()                                               = default;
    DescriptorAllocatorGrowable(const DescriptorAllocatorGrowable &)            = delete;
    DescriptorAllocatorGrowable &operator=(const DescriptorAllocatorGrowable &) = delete;

    // Initialise l'allocateur et crée le premier pool de 'initialSets' sets.
    // Les ratios définissent combien de descriptors de chaque type sont réservés par set.
    void init(Device &device, uint32_t initialSets, const std::vector<PoolSizeRatio> &poolRatios);

    // Remet tous les pools à zéro (les sets alloués deviennent invalides)
    // et les replace dans la liste des pools disponibles. Ne détruit rien.
    void clearPools();

    // Détruit tous les pools Vulkan. À appeler avant la destruction du Device.
    void destroyPools();

    // Alloue un descriptor set depuis le layout donné.
    // Si le pool courant est plein, un nouveau pool est créé automatiquement.
    // Retourne false uniquement si la création d'un nouveau pool échoue.
    bool allocate(VkDescriptorSetLayout layout, VkDescriptorSet &set);

  private:
    // Retourne un pool disponible depuis readyPools_, ou en crée un nouveau si vide.
    VkDescriptorPool getOrCreatePool();

    // Crée un VkDescriptorPool pouvant accueillir 'setCount' sets,
    // avec les tailles calculées depuis ratios_.
    VkDescriptorPool createPool(uint32_t setCount);

    Device                    *device_ = nullptr;
    std::vector<PoolSizeRatio> ratios_;

    // Pools ayant encore de la capacité disponible.
    std::vector<VkDescriptorPool> readyPools_;

    // Pools épuisés — conservés pour être réinitialisés lors d'un clearPools().
    std::vector<VkDescriptorPool> fullPools_;

    // Taille du prochain pool à créer — double à chaque création, max 4096.
    uint32_t setsPerPool_ = 0;
  };

  // Construit et envoie les VkWriteDescriptorSet vers le GPU.
  // S'utilise en chaîne après avoir préparé les VkDescriptorBufferInfo / VkDescriptorImageInfo :
  //
  //   DescriptorWriter{layout, pool}
  //     .writeBuffer(0, &bufferInfo)
  //     .writeImage(1, &imageInfo)
  //     .build(set);
  //
  // Pour les allocations dynamiques, utiliser l'overload build(set, allocator).
  class DescriptorWriter {
  public:
    DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool);
    DescriptorWriter(DescriptorSetLayout &setLayout);

    // Prépare l'écriture d'un buffer (UBO, SSBO...) au binding donné.
    DescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);

    // Prépare l'écriture d'une image (sampler, storage image...) au binding donné.
    DescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

    // Alloue un descriptor set depuis le DescriptorPool fixe passé au constructeur,
    // puis y écrit tous les descriptors préparés. Retourne false si le pool est plein.
    bool build(VkDescriptorSet &set);

    // Alloue un descriptor set depuis un DescriptorAllocatorGrowable (capacité illimitée),
    // puis y écrit tous les descriptors préparés. À privilégier pour les matériaux.
    bool build(VkDescriptorSet &set, DescriptorAllocatorGrowable &allocator);

    // Réécrit les descriptors dans un set déjà alloué, sans réallocation.
    // Utile pour mettre à jour un set existant (ex : changement de texture).
    void overwrite(VkDescriptorSet &set);

  private:
    DescriptorSetLayout              &setLayout;
    DescriptorPool                   *pool;
    std::vector<VkWriteDescriptorSet> writes;
  };

} // namespace mc
