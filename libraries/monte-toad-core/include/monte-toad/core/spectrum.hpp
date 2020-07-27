#pragma once

namespace mt::core {
  struct BsdfSampleInfo {
    glm::vec3 wo = glm::vec3(0.0f), fs = glm::vec3(0.0f);
    float pdf = 0.0f;
  };
}
