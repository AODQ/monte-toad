#include <monte-toad/core/math.hpp>

//------------------------------------------------------------------------------
bool glm::GreaterThan(glm::vec3 a, glm::vec3 b) {
  return (a.x > b.x) || (a.y > b.y) || (a.z > b.z);
}

//------------------------------------------------------------------------------
float CosDPhi(glm::vec3 const & wi, glm::vec3 const & wo) {
  // ф can be calculated by zering Z coordinate of the two vectors to get 2D
  // vectors, which can ben be normalized; their dot product gives the cosine
  // angle between them.
  // -- from PBRT-v3

  return
    glm::clamp(

      (wi.x*wo.x + wo.y*wo.y)
    / glm::sqrt(
        (wi.x*wi.x + wi.y*wi.y)
      * (wo.x*wo.x + wo.y*wo.y)
      )

    , -1.0f
    , +1.0f
    );
}
