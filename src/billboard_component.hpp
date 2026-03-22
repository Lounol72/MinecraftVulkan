#pragma once

#include <memory>
#include "material.hpp"

namespace mc {
  enum BillboardType { SCREEN_ALIGNED, VIEWPOINT_ORIENTED, CYLINDRICAL };
  struct BillboardComponent {
    BillboardType             type     = BillboardType::SCREEN_ALIGNED;
    float                     radius   = 0.1f;
    std::shared_ptr<Material> material = nullptr;
  };
} // namespace mc
