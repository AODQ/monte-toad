#pragma once

namespace mt::core { struct Scene; }
namespace mt::core { struct Triangle; }
namespace mt::core { struct BvhIntersection; }

namespace mt::core {
  struct SurfaceInfo {
    SurfaceInfo() = default;

    mt::core::Triangle const * triangle = nullptr;
    float distance = 0.0f;
    glm::vec2 barycentricUv = glm::vec2(0.0f);

    glm::vec3 origin = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    glm::vec2 uvcoord = glm::vec3(0.0f);

    bool exitting = false;

    glm::vec3 incomingAngle = glm::vec3(0.0f);

    size_t material;

    bool Valid() { return triangle != nullptr; }

    // creates an invalid surface; not triangle was hit
    static SurfaceInfo Construct(glm::vec3 origin, glm::vec3 incomingAngle);

    // creates a valid surface; specificying a single point on a triangle at an
    // incoming angle.
    static SurfaceInfo Construct(
      mt::core::Scene const & scene
    , mt::core::Triangle const * triangle
    , mt::core::BvhIntersection const & intersection
    , glm::vec3 origin
    , glm::vec3 incomingAngle
    );
  };
}
