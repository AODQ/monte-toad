#pragma once

#include "../ext/bvh.hpp"
#include <glm/glm.hpp>

struct Triangle {
  // TODO I wish i could just do Triangle() = default, but it does not construct
  // properly for some reason
  Triangle(
    uint16_t meshIdx_
  , glm::vec3 a_, glm::vec3 b_, glm::vec3 c_
  , glm::vec3 n0_, glm::vec3 n1_, glm::vec3 n2_
  , glm::vec2 uv0_, glm::vec2 uv1_, glm::vec2 uv2_
  );
  Triangle();

  ~Triangle();

  uint16_t meshIdx;

  glm::vec3 v0, v1, v2;
  glm::vec3 n0, n1, n2;
  glm::vec2 uv0, uv1, uv2;
};

struct Intersection {
  Intersection() = default;
  float distance;
  glm::vec2 barycentricUv;
};

enum class CullFace { None, Front, Back };

std::optional<Intersection> RayTriangleIntersection(
  glm::vec3 ori
, glm::vec3 dir
, Triangle const & triangle
, CullFace cullFace = CullFace::None
, float epsilon = 0.0000001f
);

struct AccelerationStructure {
  AccelerationStructure() = default;

  std::vector<Triangle> triangles;
  bvh::BVH<> boundingVolume;

  static AccelerationStructure Construct(
    std::vector<Triangle> && triangles,
    bool optimize = false
  );
};

std::optional<std::pair<size_t, Intersection>> IntersectClosest(
  AccelerationStructure const & accel
, glm::vec3 ori, glm::vec3 dir
);
