#pragma once

#include <vector>
#include "texture.hpp"

namespace mc {
  struct Material {
    std::vector<Texture> textures{};
  };
} // namespace mc
