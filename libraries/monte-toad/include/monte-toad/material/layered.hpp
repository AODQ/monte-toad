#pragma once

#include <monte-toad/core/math.hpp>

#include <vector>

// implements layered material as laid out in
//  "efficient rendering of layered materials using an atomic decomposition
//   with statistical operators"

namespace mt::core { struct SurfaceInfo; }
namespace mt { struct PluginInfoRandom; }

namespace mt::material::layered {

  struct FresnelMicrofacetTable {
    std::vector<float> buffer;
    // each element corresponds to an index T, A, N, K
    glm::u32vec4 size;
    glm::vec4 min, max;

    // if buffer is empty use this
    glm::vec3 fresnelConstant = glm::vec3(0.0f);

    float Get(glm::vec4 const & idx) const;
    glm::vec3 Get(
      float t
    , float a
    , glm::vec3 const & n
    , glm::vec3 const & k
    ) const;
  };

  struct TotalInternalReflectionTable {
    std::vector<float> buffer;
    // each element corresponds to an index T, A, N
    glm::u32vec3 size;
    glm::vec3 min, max;

    // if buffer is empty use this
    float tirConstant = 0.0f;

    float Get(glm::vec3 const & idx) const;
  };


  struct Data {
    struct Layer {
      float depth = 0.0f;
      float sigmaA = 0.0f;
      float sigmaS = 0.0f;
      float g = 0.0f;

      glm::vec3 ior = glm::vec3(1.0f);
      glm::vec3 kappa = glm::vec3(1.0f);
      float alpha = 0.0f;
    };

    std::vector<Layer> layers;

    mt::material::layered::FresnelMicrofacetTable fresnelTable;
    mt::material::layered::TotalInternalReflectionTable tirTable;
  };

  std::tuple<glm::vec3 /*wo*/, glm::vec3 /*bsdfFs*/, float /*pdf*/>
  BsdfSample(
    mt::material::layered::Data const & structure
  , mt::PluginInfoRandom const & random
  , mt::core::SurfaceInfo const & surface
  );

  glm::vec3 BsdfFs(
    mt::material::layered::Data const & structure
  , mt::core::SurfaceInfo const & surface
  , glm::vec3 const & wo
  );

  float BsdfPdf(
    mt::material::layered::Data const & structure
  , mt::core::SurfaceInfo const & surface
  , glm::vec3 const & wo
  );
}
