#include <monte-toad/surfaceinfo.hpp>

#include <monte-toad/geometry.hpp>
#include <monte-toad/primitive.hpp>
#include <monte-toad/scene.hpp>

mt::SurfaceInfo mt::SurfaceInfo::Construct(
  glm::vec3 origin
, glm::vec3 incomingAngle
) {
  mt::SurfaceInfo surface;
  surface.origin = origin;
  surface.incomingAngle = incomingAngle;
  return surface;
}

mt::SurfaceInfo mt::SurfaceInfo::Construct(
  mt::Scene const & /*scene*/
, mt::Triangle const * triangle
, mt::BvhIntersection const & intersection
, glm::vec3 origin
, glm::vec3 incomingAngle
) {
  mt::SurfaceInfo surface;
  surface.triangle = triangle;
  surface.origin = origin;
  surface.incomingAngle = incomingAngle;
  surface.distance = intersection.length;
  surface.barycentricUv = intersection.barycentricUv;
  surface.material = surface.triangle->meshIdx;

  if (triangle) {
    surface.normal =
      BarycentricInterpolation(
        triangle->n0, triangle->n1, triangle->n2, surface.barycentricUv
      );
    surface.uvcoord =
      BarycentricInterpolation(
        triangle->uv0, triangle->uv1, triangle->uv2, surface.barycentricUv
      );

    // flip normal if surface is being exitted
    if (glm::dot(-surface.incomingAngle, surface.normal) < 0.0f) {
      surface.normal *= -1.0f;
      surface.exitting = true;
    }
  }

  return surface;
}
