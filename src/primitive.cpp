#include "primitive.hpp"

#include <spdlog/spdlog.h>

#include "span.hpp"

////////////////////////////////////////////////////////////////////////////////
Triangle::Triangle() {}

////////////////////////////////////////////////////////////////////////////////
Triangle::Triangle(
    uint16_t meshIdx_
  , glm::vec3 a_, glm::vec3 b_, glm::vec3 c_
  , glm::vec3 n0_, glm::vec3 n1_, glm::vec3 n2_
  , glm::vec2 uv0_, glm::vec2 uv1_, glm::vec2 uv2_
) :
    meshIdx(meshIdx_)
  , v0(a_), v1(b_), v2(c_)
  , n0(n0_), n1(n1_), n2(n2_)
  , uv0(uv0_), uv1(uv1_), uv2(uv2_)
{}

////////////////////////////////////////////////////////////////////////////////
Triangle::~Triangle() {}

////////////////////////////////////////////////////////////////////////////////
// TODO support min/max distance
std::optional<Intersection> RayTriangleIntersection(
  glm::vec3 ori
, glm::vec3 dir
, Triangle const & triangle
, CullFace cullFace
, float epsilon
) {
  glm::vec3 ab = triangle.v1 - triangle.v0;
  glm::vec3 ac = triangle.v2 - triangle.v0;
  glm::vec3 pvec = glm::cross(dir, ac);
  float determinant = glm::dot(ab, pvec);

  if ((cullFace == CullFace::None  && glm::abs(determinant) <  epsilon)
   || (cullFace == CullFace::Back  &&          determinant  <  epsilon)
   || (cullFace == CullFace::Front &&          determinant  > -epsilon)
  ) {
    return std::nullopt;
  }

  float determinantInv = 1.0f / determinant;

  glm::vec2 uv;

  glm::vec3 tvec = ori - triangle.v0;
  uv.x = determinantInv * glm::dot(tvec, pvec);
  if (uv.x < 0.0f || uv.x > 1.0f) { return std::nullopt; }

  glm::vec3 qvec = glm::cross(tvec, ab);
  uv.y = determinantInv * glm::dot(dir, qvec);
  if (uv.y < 0.0f || uv.x + uv.y > 1.0f) { return std::nullopt; }

  float distance = determinantInv * glm::dot(ac, qvec);
  if (distance <= 0.0f) { return std::nullopt; }

  Intersection intersection;
  intersection.distance = distance;
  intersection.barycentricUv = uv;
  return std::make_optional(intersection);
}

////////////////////////////////////////////////////////////////////////////////
AccelerationStructure AccelerationStructure::Construct(
  std::vector<Triangle> && trianglesMv,
  bool optimize
) {
  AccelerationStructure accel;
  accel.triangles = std::move(trianglesMv);

  auto boundingBoxes = std::vector<bvh::BBox>();
  boundingBoxes.resize(accel.triangles.size());
  auto centers = std::vector<bvh::Vec3>();
  centers.resize(accel.triangles.size());

  /* #pragma omp parallel for */
  for (size_t i = 0; i < accel.triangles.size(); ++ i) {
    auto & triangle = accel.triangles[i];
    auto
      bvhV0 = bvh::Vec3(triangle.v0.x, triangle.v0.y, triangle.v0.z)
    , bvhV1 = bvh::Vec3(triangle.v1.x, triangle.v1.y, triangle.v1.z)
    , bvhV2 = bvh::Vec3(triangle.v2.x, triangle.v2.y, triangle.v2.z)
    ;

    auto bbox = bvh::BBox { bvhV0 };
    bbox.extend(bvhV1);
    bbox.extend(bvhV2);

    boundingBoxes[i] = bbox;
    centers[i]       = (bvhV0 + bvhV1 + bvhV2) * (1.0f/3.0f);
  }

  accel.boundingVolume.build(
    boundingBoxes.data(), centers.data(), accel.triangles.size()
  );
  if (optimize) { accel.boundingVolume.optimize(); }


  // Reorder primitives to BVH's primitive indices, ei before you would have to
  // access the triangles as
  //   accel.triangles[accel.boundingVolume.primitive_indices[i]]
  // now you can just do
  //   accel.triangles[i]
  decltype(accel.triangles) primitivesCopy;
  primitivesCopy.reserve(primitivesCopy.size());
  for (size_t i = 0; i < accel.triangles.size(); ++ i) {
    primitivesCopy
      .emplace_back(accel.triangles[accel.boundingVolume.primitive_indices[i]]);
  }
  std::swap(accel.triangles, primitivesCopy);

  return accel;
}

////////////////////////////////////////////////////////////////////////////////
std::optional<std::pair<size_t, Intersection>>
IntersectClosest(
  AccelerationStructure const & accel
, glm::vec3 ori, glm::vec3 dir
) {
  struct ClosestIntersector {
    ClosestIntersector() = default;

    span<Triangle const> triangles;
    size_t it = 0;

    using Result = Intersection;

    static ClosestIntersector Construct(
      AccelerationStructure const & accel
    ) {
      ClosestIntersector intersector;
      intersector.triangles = make_span(accel.triangles);
      return intersector;
    }

    std::optional<Intersection> operator()(size_t idx, bvh::Ray const & ray)
      const
    {
      return
        RayTriangleIntersection(
          glm::vec3(ray.origin.x, ray.origin.y, ray.origin.z)
        , glm::vec3(ray.direction.x, ray.direction.y, ray.direction.z)
        , triangles[idx]
        );
    }
  };

  return
    accel
      .boundingVolume
      .intersect<false, ClosestIntersector>(
        bvh::Ray(
          bvh::Vec3(ori.x, ori.y, ori.z)
        , bvh::Vec3(dir.x, dir.y, dir.z)
        )
      , ClosestIntersector::Construct(accel)
      );
}
