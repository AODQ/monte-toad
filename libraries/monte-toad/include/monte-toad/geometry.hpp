#pragma once

#include <glm/detail/type_vec2.hpp>  // for vec2
#include <glm/ext/vector_float2.hpp> // for vec2 float specialization
#include <glm/ext/vector_float3.hpp> // for vec3 float specialization

#include <tuple>

#include <monte-toad/math.hpp>
template <typename U>
U BarycentricInterpolation(
  U const & v0, U const & v1, U const & v2
, glm::vec2 const & uv
) {
  return v0 + uv.x*(v1 - v0) + uv.y*(v2 - v0);
}

std::tuple<glm::vec3 /*tangent*/, glm::vec3 /*binormal*/> OrthogonalVectors(
  glm::vec3 const & normal
);

// Reorients hemisphere of an angle to its normal
glm::vec3 ReorientHemisphere(glm::vec3 wo, glm::vec3 normal);

// Converts polar coordinates to cartesian
glm::vec3 Cartesian(float cosTheta, float phi);

float SphericalTheta(glm::vec3 const & v);

float SphericalPhi(glm::vec3 const & v);
