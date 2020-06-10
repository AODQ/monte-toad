#include <monte-toad/geometry.hpp>

//------------------------------------------------------------------------------
std::tuple<glm::vec3 /*tangent*/, glm::vec3 /*binormal*/> OrthogonalVectors(
  glm::vec3 const & normal
) {
  auto tangent =
    glm::abs(normal.x) > 0.0f
  ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
  tangent = glm::normalize(glm::cross(normal, tangent));
  glm::vec3 binormal = glm::cross(normal, tangent);
  return std::make_pair(tangent, binormal);
}

//------------------------------------------------------------------------------
glm::vec3 ReorientHemisphere(glm::vec3 wo, glm::vec3 normal) {
  auto [tangent, binormal] = OrthogonalVectors(normal);
  return tangent*wo.x + binormal*wo.y + normal*wo.z;
}

//------------------------------------------------------------------------------
glm::vec3 Cartesian(float cosTheta, float phi) {
  float sinTheta = glm::sqrt(glm::max(0.0f, 1.0f - cosTheta));
  return glm::vec3(glm::cos(phi)*sinTheta, glm::sin(phi)*sinTheta, cosTheta);
}

//------------------------------------------------------------------------------
float SphericalTheta(glm::vec3 const & v) {
  return glm::acos(glm::clamp(v.z, -1.0f, +1.0f));
}

//------------------------------------------------------------------------------
float SphericalPhi(glm::vec3 const & v) {
  float p = glm::atan(v.y, v.x);
  return p < 0.0f ? (p + glm::Tau) : p;
}
