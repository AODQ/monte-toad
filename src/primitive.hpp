#pragma once

#include <bvh/single_ray_traversal.hpp>
#include <bvh/bvh.hpp>
#include <glm/glm.hpp>

#include <memory>

constexpr static float Pi     = 3.141592653589793f;
constexpr static float InvPi  = 0.318309886183791f;
constexpr static float Tau    = 6.283185307179586f;
constexpr static float InvTau = 0.159154943091895f;

struct Camera {
  glm::vec3 ori;
  glm::vec3 lookat;
  glm::vec3 up;
  float fov;
};

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
  size_t triangleIndex;
  float length;
  glm::vec2 barycentricUv;

  float distance() const { return length; }
};

enum class CullFace { None, Front, Back };

std::optional<Intersection> RayTriangleIntersection(
  glm::vec3 ori
, glm::vec3 dir
, Triangle const & triangle
, CullFace cullFace = CullFace::None
, float epsilon = 0.00000001f
);

struct AccelerationStructure {
  AccelerationStructure() = default;

  std::vector<Triangle> triangles;
  bvh::Bvh<float> boundingVolume;
  bvh::SingleRayTraversal<decltype(boundingVolume)>
    boundingVolumeTraversal { boundingVolume };

  static std::unique_ptr<AccelerationStructure> Construct(
    std::vector<Triangle> && triangles
  , bool optimize = false
  );
};

std::optional<Intersection> IntersectClosest(
  AccelerationStructure const & accel
, glm::vec3 ori, glm::vec3 dir
);
